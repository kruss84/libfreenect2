/**
*  FreenectDriver
*  Copyright 2013 Benn Snyder <benn.snyder@gmail.com>
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*/
/**
*  OpenNI2 Freenect2 Driver
*  Copyright 2015 hanyazou@gmail.com
*/
#include <map>
#include <string>
#include <array>
#include <sys/time.h>
#include "Driver/OniDriverAPI.h"
#include "libfreenect2/libfreenect2.hpp"
#include <libfreenect2/frame_listener.hpp>
#include "DepthStream.hpp"
#include "ColorStream.hpp"
#include "IrStream.hpp"


namespace Freenect2Driver
{
  typedef std::map<std::string, std::string> ConfigStrings;

  class Device : public oni::driver::DeviceBase,  public libfreenect2::FrameListener
  {
  private:
    libfreenect2::Freenect2Device *dev;
    ColorStream* color;
    DepthStream* depth;
    IrStream* ir;
    Registration *reg;
    ConfigStrings config;

    struct timeval ts_epoc;
    long getTimestamp() {
      struct timeval ts;
      gettimeofday(&ts, NULL);
      return (ts.tv_sec - ts_epoc.tv_sec) * 1000 + ts.tv_usec / 1000;  // XXX, ignoring nsec of the epoc.
    }

    // for Freenect::FreenectDevice
    bool onNewFrame(libfreenect2::Frame::Type type, libfreenect2::Frame *frame) {
      if (type == libfreenect2::Frame::Color) {
        if (color)
          color->buildFrame(frame, getTimestamp());
      } else 
      if (type == libfreenect2::Frame::Ir) {
        if (ir)
          ir->buildFrame(frame, getTimestamp());
      } else 
      if (type == libfreenect2::Frame::Depth) {
        if (depth)
          depth->buildFrame(frame, getTimestamp());
      }
    }

    OniStatus setStreamProperties(VideoStream* stream, std::string pfx)
    {
      pfx += '-';
      OniStatus res = ONI_STATUS_OK, tmp_res;
      if (config.find(pfx + "size") != config.end()) {
        WriteMessage("setStreamProperty: " + pfx + "size: " + config[pfx + "size"]);
        std::string size(config[pfx + "size"]);
        int i = size.find("x");
        OniVideoMode video_mode = makeOniVideoMode(ONI_PIXEL_FORMAT_DEPTH_1_MM, std::stoi(size.substr(0, i)), std::stoi(size.substr(i + 1)), 30);
        tmp_res = stream->setProperty(ONI_STREAM_PROPERTY_VIDEO_MODE, (void*)&video_mode, sizeof(video_mode));
        if (tmp_res != ONI_STATUS_OK)
          res = tmp_res;
      }
    }

  public:
    Device(freenect2_context* fn_ctx, int index) : //libfreenect2::Freenect2Device(fn_ctx, index),
      dev(NULL),
      reg(NULL),
      color(NULL),
      ir(NULL),
      depth(NULL)
    {
      gettimeofday(&ts_epoc, NULL);
    }
    ~Device()
    {
      destroyStream(color);
      destroyStream(ir);
      destroyStream(depth);
      if (reg) {
        delete reg;
        reg = NULL;
      }
    }

    // for Freenect2Device
    void setFreenect2Device(libfreenect2::Freenect2Device *dev) {
      this->dev = dev;
      dev->setColorFrameListener(this);
      dev->setIrAndDepthFrameListener(this);
      reg = new Registration(dev);
    }
    void setConfigStrings(ConfigStrings& config) {
      this->config = config;
    }
    void start() { dev->start(); }
    void stop() { dev->stop(); }
    void close() { dev->close(); }

    // for DeviceBase

    OniBool isImageRegistrationModeSupported(OniImageRegistrationMode mode) { return depth->isImageRegistrationModeSupported(mode); }

    OniStatus getSensorInfoList(OniSensorInfo** pSensors, int* numSensors)
    {
      *numSensors = 3;
      OniSensorInfo * sensors = new OniSensorInfo[*numSensors];
      sensors[0] = depth->getSensorInfo();
      sensors[1] = color->getSensorInfo();
      sensors[2] = ir->getSensorInfo();
      *pSensors = sensors;
      return ONI_STATUS_OK;
    }

    oni::driver::StreamBase* createStream(OniSensorType sensorType)
    {
      switch (sensorType)
      {
        default:
          LogError("Cannot create a stream of type " + to_string(sensorType));
          return NULL;
        case ONI_SENSOR_COLOR:
          if (! color) {
            color = new ColorStream(dev, reg);
            setStreamProperties(color, "color");
          }
          return color;
        case ONI_SENSOR_DEPTH:
          if (! depth) {
            depth = new DepthStream(dev, reg);
            setStreamProperties(depth, "depth");
          }
          return depth;
        case ONI_SENSOR_IR:
          if (! ir) {
            ir = new IrStream(dev, reg);
            setStreamProperties(ir, "ir");
          }
          return ir;
      }
    }

    void destroyStream(oni::driver::StreamBase* pStream)
    {
      if (pStream == NULL)
        return;

      // stop them all
      dev->stop();
      if (pStream == color)
      {
        delete color;
        color = NULL;
      }
      if (pStream == depth)
      {
        delete depth;
        depth = NULL;
      }
      if (pStream == ir)
      {
        delete ir;
        ir = NULL;
      }
    }

    // todo: fill out properties
    OniBool isPropertySupported(int propertyId)
    {
      if (propertyId == ONI_DEVICE_PROPERTY_IMAGE_REGISTRATION)
        return true;
      return false;
    }

    OniStatus getProperty(int propertyId, void* data, int* pDataSize)
    {
      switch (propertyId)
      {
        default:
        case ONI_DEVICE_PROPERTY_FIRMWARE_VERSION:        // string
        case ONI_DEVICE_PROPERTY_DRIVER_VERSION:          // OniVersion
        case ONI_DEVICE_PROPERTY_HARDWARE_VERSION:        // int
        case ONI_DEVICE_PROPERTY_SERIAL_NUMBER:           // string
        case ONI_DEVICE_PROPERTY_ERROR_STATE:             // ?
        // files
        case ONI_DEVICE_PROPERTY_PLAYBACK_SPEED:          // float
        case ONI_DEVICE_PROPERTY_PLAYBACK_REPEAT_ENABLED: // OniBool
        // xn
        case XN_MODULE_PROPERTY_USB_INTERFACE:            // XnSensorUsbInterface
        case XN_MODULE_PROPERTY_MIRROR:                   // bool
        case XN_MODULE_PROPERTY_RESET_SENSOR_ON_STARTUP:  // unsigned long long
        case XN_MODULE_PROPERTY_LEAN_INIT:                // unsigned long long
        case XN_MODULE_PROPERTY_SERIAL_NUMBER:            // unsigned long long
        case XN_MODULE_PROPERTY_VERSION:                  // XnVersions
          return ONI_STATUS_NOT_SUPPORTED;

        case ONI_DEVICE_PROPERTY_IMAGE_REGISTRATION:      // OniImageRegistrationMode
          if (*pDataSize != sizeof(OniImageRegistrationMode))
          {
            LogError("Unexpected size for ONI_DEVICE_PROPERTY_IMAGE_REGISTRATION");
            return ONI_STATUS_ERROR;
          }
          *(static_cast<OniImageRegistrationMode*>(data)) = depth->getImageRegistrationMode();
          return ONI_STATUS_OK;
      }
    }
    
    OniStatus setProperty(int propertyId, const void* data, int dataSize)
    {
      switch (propertyId)
      {
        default:
        case ONI_DEVICE_PROPERTY_FIRMWARE_VERSION:        // By implementation
        case ONI_DEVICE_PROPERTY_DRIVER_VERSION:          // OniVersion
        case ONI_DEVICE_PROPERTY_HARDWARE_VERSION:        // int
        case ONI_DEVICE_PROPERTY_SERIAL_NUMBER:           // string
        case ONI_DEVICE_PROPERTY_ERROR_STATE:             // ?
        // files
        case ONI_DEVICE_PROPERTY_PLAYBACK_SPEED:          // float
        case ONI_DEVICE_PROPERTY_PLAYBACK_REPEAT_ENABLED: // OniBool
        // xn
        case XN_MODULE_PROPERTY_USB_INTERFACE:            // XnSensorUsbInterface
        case XN_MODULE_PROPERTY_MIRROR:                   // bool
        case XN_MODULE_PROPERTY_RESET_SENSOR_ON_STARTUP:  // unsigned long long
        case XN_MODULE_PROPERTY_LEAN_INIT:                // unsigned long long
        case XN_MODULE_PROPERTY_SERIAL_NUMBER:            // unsigned long long
        case XN_MODULE_PROPERTY_VERSION:                  // XnVersions
        // xn commands
        case XN_MODULE_PROPERTY_FIRMWARE_PARAM:           // XnInnerParam
        case XN_MODULE_PROPERTY_RESET:                    // unsigned long long
        case XN_MODULE_PROPERTY_IMAGE_CONTROL:            // XnControlProcessingData
        case XN_MODULE_PROPERTY_DEPTH_CONTROL:            // XnControlProcessingData
        case XN_MODULE_PROPERTY_AHB:                      // XnAHBData
        case XN_MODULE_PROPERTY_LED_STATE:                // XnLedState
          return ONI_STATUS_NOT_SUPPORTED;

        case ONI_DEVICE_PROPERTY_IMAGE_REGISTRATION:      // OniImageRegistrationMode
          if (dataSize != sizeof(OniImageRegistrationMode))
          {
            LogError("Unexpected size for ONI_DEVICE_PROPERTY_IMAGE_REGISTRATION");
            return ONI_STATUS_ERROR;
          }
          return depth->setImageRegistrationMode(*(static_cast<const OniImageRegistrationMode*>(data)));
      }
    }

    OniBool isCommandSupported(int commandId)
    {
      switch (commandId)
      {
        default:
        case ONI_DEVICE_COMMAND_SEEK:
          return false;
      }
    }
    
    OniStatus invoke(int commandId, void* data, int dataSize)
    {
      switch (commandId)
      {
        default:
        case ONI_DEVICE_COMMAND_SEEK: // OniSeek
          return ONI_STATUS_NOT_SUPPORTED;
      }
    }


    /* todo: for DeviceBase
    virtual OniStatus tryManualTrigger() {return ONI_STATUS_OK;}
    */
  };


  class Driver : public oni::driver::DriverBase, private libfreenect2::Freenect2
  {
  private:
    typedef std::map<OniDeviceInfo, oni::driver::DeviceBase*> OniDeviceMap;
    OniDeviceMap devices;
    std::string uriScheme;
    ConfigStrings config;

    std::string devid_to_uri(int id) {
      return uriScheme + "://" + to_string(id);
    }

    int uri_to_devid(const std::string uri) {
      int id;
      std::istringstream is(uri);
      is.seekg((uriScheme + "://").length());
      is >> id;
      return id;
    }

    void register_uri(std::string uri) {
      OniDeviceInfo info;
      strncpy(info.uri, uri.c_str(), ONI_MAX_STR);
      strncpy(info.vendor, "Microsoft", ONI_MAX_STR);
      //strncpy(info.name, "Kinect 2", ONI_MAX_STR); // XXX, NiTE does not accept new name
      strncpy(info.name, "Kinect", ONI_MAX_STR);
      if (devices.find(info) == devices.end()) {
        WriteMessage("Driver: register new uri: " + uri);
        devices[info] = NULL;
        deviceConnected(&info);
        deviceStateChanged(&info, 0);
      }
    }

  public:
    Driver(OniDriverServices* pDriverServices) : DriverBase(pDriverServices),
      uriScheme("freenect2")
    {
        //WriteMessage("Using libfreenect v" + to_string(PROJECT_VER));
      WriteMessage("Using libfreenect2");

#if 0
      freenect_set_log_level(m_ctx, FREENECT_LOG_DEBUG);
      freenect_select_subdevices(m_ctx, FREENECT_DEVICE_CAMERA); // OpenNI2 doesn't use MOTOR or AUDIO
#endif // 0
      DriverServices = &getServices();
    }
    ~Driver() { shutdown(); }

    // for DriverBase

    OniStatus initialize(oni::driver::DeviceConnectedCallback connectedCallback, oni::driver::DeviceDisconnectedCallback disconnectedCallback, oni::driver::DeviceStateChangedCallback deviceStateChangedCallback, void* pCookie)
    {
      DriverBase::initialize(connectedCallback, disconnectedCallback, deviceStateChangedCallback, pCookie);
      for (int i = 0; i < Freenect2::enumerateDevices(); i++)
      {
        std::string uri = devid_to_uri(i);
        std::array<std::string, 3> modes = {
          "",
          "?depth-size=640x480",
          "?depth-size=512x424",
        };

        WriteMessage("Found device " + uri);

        for (int i = 0; i < modes.size(); i++) {
          register_uri(uri + modes[i]);
        }

#if 0
        freenect_device* dev;
        if (freenect_open_device(m_ctx, &dev, i) == 0)
        {
          info.usbVendorId = dev->usb_cam.VID;
          info.usbProductId = dev->usb_cam.PID;
          freenect_close_device(dev);
        }
        else
        {
          WriteMessage("Unable to open device to query VID/PID");
        }
#endif // 0
      }
      return ONI_STATUS_OK;
    }

    oni::driver::DeviceBase* deviceOpen(const char* c_uri, const char* c_mode = NULL)
    {
      std::string uri(c_uri);
      std::string mode(c_mode ? c_mode : "");
      if (uri.find("?") != -1) {
        mode += "&";
        mode += uri.substr(uri.find("?") + 1);
        uri = uri.substr(0, uri.find("?"));
      }
      std::stringstream ss(mode);
      std::string buf;
      while(std::getline(ss, buf, '&')) {
        if (buf.find("=") != -1) {
          config[buf.substr(0, buf.find("="))] = buf.substr(buf.find("=")+1);
        } else {
          if (0 < buf.length())
            config[buf] = "";
        }
      }
      WriteMessage("deiveOpen: " + uri);
      for (std::map<std::string, std::string>::iterator it = config.begin(); it != config.end(); it++) {
        WriteMessage("    " + it->first + " = " + it->second);
      }

      for (OniDeviceMap::iterator iter = devices.begin(); iter != devices.end(); iter++)
      {
        std::string iter_uri(iter->first.uri);
        if (iter_uri.substr(0, iter_uri.find("?")) == uri) // found
        {
          if (iter->second) // already open
          {
            return iter->second;
          }
          else 
          {
            WriteMessage("Opening device " + std::string(uri));
            int id = uri_to_devid(iter->first.uri);
            Device* device = new Device(NULL, id);
            device->setFreenect2Device(openDevice(id)); // XXX, detault pipeline // const PacketPipeline *factory);
            device->setConfigStrings(config);
            iter->second = device;
            return device;
          }
        }
      }

      LogError("Could not find device " + std::string(uri));
      return NULL;
    }

    void deviceClose(oni::driver::DeviceBase* pDevice)
    {
      for (OniDeviceMap::iterator iter = devices.begin(); iter != devices.end(); iter++)
      {
        if (iter->second == pDevice)
        {
          WriteMessage("Closing device " + std::string(iter->first.uri));
          int id = uri_to_devid(iter->first.uri);
          devices.erase(iter);
          Device* device = (Device*)iter->second;
          device->stop();
          device->close();
          return;
        }
      }

      LogError("Could not close unrecognized device");
    }

    OniStatus tryDevice(const char* uri)
    {
      oni::driver::DeviceBase* device = deviceOpen(uri);
      if (! device)
        return ONI_STATUS_ERROR;
      deviceClose(device);
      register_uri(std::string(uri)); // XXX, register new uri here.
      return ONI_STATUS_OK;
    }

    void shutdown()
    {
      for (OniDeviceMap::iterator iter = devices.begin(); iter != devices.end(); iter++)
      {
        WriteMessage("Closing device " + std::string(iter->first.uri));
        int id = uri_to_devid(iter->first.uri);
        Device* device = (Device*)iter->second;
        device->stop();
        device->close();
      }

      devices.clear();
    }


    /* todo: for DriverBase
    virtual void* enableFrameSync(oni::driver::StreamBase** pStreams, int streamCount);
    virtual void disableFrameSync(void* frameSyncGroup);
    */
  };
}


// macros defined in XnLib (not included) - workaround
#define XN_NEW(type, arg...) new type(arg)
#define XN_DELETE(p) delete(p)
ONI_EXPORT_DRIVER(Freenect2Driver::Driver);
#undef XN_NEW
#undef XN_DELETE
