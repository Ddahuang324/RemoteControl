
// encoding: UTF-8
#include "pch.h"
#include "MoniterModel.h"
#include "Interface.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <wincodec.h>
#include <wrl.h>


#pragma comment(lib, "Windowscodecs.lib")

MonitorModel::MonitorModel(std::shared_ptr<ThreadPool> pool) {
  if (!m_screenBuffer)
    m_screenBuffer = std::make_unique<MonitorProtocol::ScreenBuffer>();
  if (pool)
    m_screenBuffer->pool = std::move(pool);
  if (!m_monitorRes)
    m_monitorRes = std::make_unique<MonitorProtocol::MonitorResources>();
}

using Microsoft::WRL::ComPtr;

// Define IDecoder in global namespace to match InterfaceProtocol
// forward-declaration
struct IDecoder {
  virtual ~IDecoder() = default;
  virtual bool DecodeToRGBA(const std::vector<uint8_t> &compressed,
                            std::vector<uint8_t> &outPixels, int &outWidth,
                            int &outHeight) = 0;
};

namespace {

class LocalWicDecoder : public ::IDecoder {
public:
  LocalWicDecoder() {
    factory_ = nullptr;
    HRESULT hr =
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&factory_));
    if (FAILED(hr)) {
      factory_ = nullptr;
    }
  }

  ~LocalWicDecoder() override {
    if (factory_) {
      factory_->Release();
      factory_ = nullptr;
    }
  }

  bool DecodeToRGBA(const std::vector<uint8_t> &compressed,
                    std::vector<uint8_t> &outPixels, int &outWidth,
                    int &outHeight) override {
    if (!factory_)
      return false;
    if (compressed.empty())
      return false;

    HRESULT hr = S_OK;
    ComPtr<IWICStream> stream;
    hr = factory_->CreateStream(&stream);
    if (FAILED(hr))
      return false;

    hr = stream->InitializeFromMemory(const_cast<BYTE *>(compressed.data()),
                                      (UINT)compressed.size());
    if (FAILED(hr))
      return false;

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory_->CreateDecoderFromStream(
        stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
      return false;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
      return false;

    UINT w = 0, h = 0;
    hr = frame->GetSize(&w, &h);
    if (FAILED(hr) || w == 0 || h == 0)
      return false;

    ComPtr<IWICFormatConverter> converter;
    hr = factory_->CreateFormatConverter(&converter);
    if (FAILED(hr))
      return false;

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
      return false;

    outWidth = static_cast<int>(w);
    outHeight = static_cast<int>(h);
    outPixels.assign(outWidth * outHeight * 4, 0);

    hr = converter->CopyPixels(nullptr, outWidth * 4, (UINT)outPixels.size(),
                               outPixels.data());
    if (FAILED(hr))
      return false;

    return true;
  }

private:
  IWICImagingFactory *factory_ = nullptr;
};

} // namespace

// Helper: save RGBA buffer (32bpp RGBA) to PNG using WIC. Returns true on
// success.
static bool SaveRgbaToPng(const std::vector<uint8_t> &rgba, int width,
                          int height, const std::string &path) {
  if (width <= 0 || height <= 0)
    return false;
  if ((int)rgba.size() < width * height * 4)
    return false;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool comInited = SUCCEEDED(hr);

  IWICImagingFactory *factory = nullptr;
  hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&factory));
  if (FAILED(hr) || !factory) {
    if (comInited)
      CoUninitialize();
    return false;
  }

  IWICStream *stream = nullptr;
  hr = factory->CreateStream(&stream);
  if (FAILED(hr) || !stream) {
    factory->Release();
    if (comInited)
      CoUninitialize();
    return false;
  }

  std::wstring wpath(path.begin(), path.end());
  hr = stream->InitializeFromFilename(wpath.c_str(), GENERIC_WRITE);
  if (FAILED(hr)) {
    stream->Release();
    factory->Release();
    if (comInited)
      CoUninitialize();
    return false;
  }

  IWICBitmapEncoder *encoder = nullptr;
  hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
  if (FAILED(hr) || !encoder) {
    stream->Release();
    factory->Release();
    if (comInited)
      CoUninitialize();
    return false;
  }

  hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
  IWICBitmapFrameEncode *frame = nullptr;
  IPropertyBag2 *props = nullptr;
  hr = encoder->CreateNewFrame(&frame, &props);
  if (FAILED(hr) || !frame) {
    if (props)
      props->Release();
    encoder->Release();
    stream->Release();
    factory->Release();
    if (comInited)
      CoUninitialize();
    return false;
  }

  hr = frame->Initialize(props);
  hr = frame->SetSize((UINT)width, (UINT)height);
  WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
  hr = frame->SetPixelFormat(&format);

  UINT stride = (UINT)(width * 4);
  hr = frame->WritePixels((UINT)height, stride, (UINT)rgba.size(),
                          const_cast<BYTE *>(rgba.data()));

  hr = frame->Commit();
  hr = encoder->Commit();

  if (props)
    props->Release();
  if (frame)
    frame->Release();
  if (encoder)
    encoder->Release();
  if (stream)
    stream->Release();
  if (factory)
    factory->Release();

  if (comInited)
    CoUninitialize();

  return SUCCEEDED(hr);
}

MonitorModel::~MonitorModel() { stopCapture(); }

void MonitorModel::startRecording(const std::string &dir) {
  if (!m_monitorRes)
    m_monitorRes = std::make_unique<MonitorProtocol::MonitorResources>();
  std::lock_guard<std::mutex> lg(m_monitorRes->recordMutex);
  if (!dir.empty())
    m_monitorRes->recordDir = dir;
  else
    m_monitorRes->recordDir = "recordings";
  m_monitorRes->recordFrameIndex.store(0);
  m_monitorRes->recording.store(true);
}

void MonitorModel::stopRecording() {
  if (m_monitorRes)
    m_monitorRes->recording.store(false);
}

void MonitorModel::startCapture(int fps, FrameCb cb) {
  if (!m_monitorRes)
    m_monitorRes = std::make_unique<MonitorProtocol::MonitorResources>();
  m_monitorRes->fps = fps > 0 ? fps : 10;
  m_monitorRes->frameCb = std::move(cb);
  m_monitorRes->running.store(true);

  // Start network consumer thread (owned by protocol resources)
  m_monitorRes->netThread = std::thread([this]() {
    while (m_monitorRes->running.load()) {
      // Wait for packet in inherited buffer
      std::unique_lock<std::mutex> lk(m_netBuffer->queueMutex);
      m_netBuffer->queueCv.wait(lk, [&] {
        return !m_netBuffer->packetQueue.empty() ||
               !m_monitorRes->running.load();
      });
      if (!m_monitorRes->running.load())
        break;
      // Pop packet
      Packet pkt = std::move(m_netBuffer->packetQueue.front());
      m_netBuffer->packetQueue.pop_front();
      lk.unlock();

      // Parse packet -> ScreenPacketData
      MonitorProtocol::ScreenPacketData spd;
      bool ok = false;
      try {
        if (pkt.sCmd == static_cast<WORD>(CMD_SCREEN_CAPTURE) ||
            pkt.sCmd == static_cast<WORD>(CMD_SCREEN_DIFF)) {
          const std::vector<uint8_t> &data = pkt.data;
          if (data.size() >= 16) {
            int x = 0, y = 0, w = 0, h = 0;
            memcpy(&x, data.data(), 4);
            memcpy(&y, data.data() + 4, 4);
            memcpy(&w, data.data() + 8, 4);
            memcpy(&h, data.data() + 12, 4);
            spd.roi_x = x;
            spd.roi_y = y;
            spd.roi_w = w;
            spd.roi_h = h;
            size_t payloadSize = data.size() - 16;
            if (payloadSize > 0) {
              spd.compressed = true;
              spd.compressedPayload.assign(data.begin() + 16, data.end());
              spd.mimeType = "image/png";
              spd.isFullFrame = (x == 0 && y == 0);
              spd.width = w;
              spd.height = h;
              ok = true;
            }
          }
        }
      } catch (...) {
        ok = false;
      }

      if (!ok) {
        // ignore unsupported packets
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      // Push to screen buffer (bounded; drop oldest when full)
      {
        std::lock_guard<std::mutex> gl(m_screenBuffer->screenQueueMutex);
        if (m_screenBuffer->screenPacketQueue.size() >=
            m_screenBuffer->maxScreenQueue) {
          m_screenBuffer->screenPacketQueue.pop_front();
        }
        m_screenBuffer->screenPacketQueue.push_back(std::move(spd));
      }
      // Notify decoder
      m_monitorRes->screenCv.notify_one();
    }
  });

  // create decoder (WIC) implementation and attach to protocol-owned decoder
  // (IDecoder)
  try {
    m_monitorRes->decoder = std::make_shared<LocalWicDecoder>();
  } catch (...) {
    m_monitorRes->decoder.reset();
  }

  // Start decoder thread: decode compressed payload -> FrameData (owned by
  // protocol)
  m_monitorRes->decodeThread = std::thread([this]() {
    while (m_monitorRes->running.load()) {
      MonitorProtocol::ScreenPacketData spd;
      {
        std::unique_lock<std::mutex> lk(m_monitorRes->screenCvMutex);
        m_monitorRes->screenCv.wait_for(lk, std::chrono::milliseconds(100));
      }
      {
        std::lock_guard<std::mutex> gl(m_screenBuffer->screenQueueMutex);
        if (m_screenBuffer->screenPacketQueue.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }
        spd = std::move(m_screenBuffer->screenPacketQueue.front());
        m_screenBuffer->screenPacketQueue.pop_front();
      }

      // decode if compressed and decoder available
      std::vector<uint8_t> rgba;
      int w = spd.width;
      int h = spd.height;
      bool decoded = false;
      if (spd.compressed && m_monitorRes->decoder) {
        // Ensure COM initialized for this thread (WIC uses COM)
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool comInited = SUCCEEDED(hr);
        try {
          decoded = m_monitorRes->decoder->DecodeToRGBA(spd.compressedPayload,
                                                        rgba, w, h);
        } catch (...) {
          decoded = false;
        }
        if (comInited)
          CoUninitialize();
      }

      // Compose ROI into canvas (model-owned) and emit full RGBA frame
      if (decoded) {
        // Use roi width/height for placement; decoded w/h may match
        int roiW = spd.roi_w;
        int roiH = spd.roi_h;
        if (roiW <= 0)
          roiW = w;
        if (roiH <= 0)
          roiH = h;

        // If full frame, initialize or resize canvas
        if (spd.isFullFrame) {
          std::lock_guard<std::mutex> gl(m_monitorRes->canvasMutex);
          m_monitorRes->canvasW = w;
          m_monitorRes->canvasH = h;
          m_monitorRes->canvas.assign(
              m_monitorRes->canvasW * m_monitorRes->canvasH * 4, 0);
          // copy full decoded image into canvas
          if ((int)rgba.size() >=
              m_monitorRes->canvasW * m_monitorRes->canvasH * 4) {
            std::copy(rgba.begin(), rgba.end(), m_monitorRes->canvas.begin());
          }
        } else {
          // ROI: composite into existing canvas if initialized
          std::lock_guard<std::mutex> gl(m_monitorRes->canvasMutex);
          if (m_monitorRes->canvasW > 0 && m_monitorRes->canvasH > 0) {
            int destX = spd.roi_x;
            int destY = spd.roi_y;
            int copyW = (std::min)(roiW, m_monitorRes->canvasW - destX);
            int copyH = (std::min)(roiH, m_monitorRes->canvasH - destY);
            if (copyW > 0 && copyH > 0) {
              for (int row = 0; row < copyH; ++row) {
                size_t srcOffset = (size_t)row * (size_t)roiW * 4;
                size_t dstOffset =
                    ((size_t)(destY + row) * (size_t)m_monitorRes->canvasW +
                     (size_t)destX) *
                    4;
                std::copy(rgba.begin() + srcOffset,
                          rgba.begin() + srcOffset + copyW * 4,
                          m_monitorRes->canvas.begin() + dstOffset);
              }
            }
          } else {
            // Canvas not initialized yet; skip until full frame arrives
          }
        }

        // Prepare FrameData containing the full canvas (copy under lock)
        if (m_monitorRes->frameCb) {
          auto frame = std::make_shared<FrameData>();
          {
            std::lock_guard<std::mutex> gl(m_monitorRes->canvasMutex);
            if (m_monitorRes->canvasW > 0 && m_monitorRes->canvasH > 0) {
              frame->width = m_monitorRes->canvasW;
              frame->height = m_monitorRes->canvasH;
              frame->rgba = m_monitorRes->canvas; // copy
            } else {
              // fallback to decoded block if canvas unavailable
              frame->width = w;
              frame->height = h;
              frame->rgba = rgba;
            }
          }
          frame->timestampMs = spd.timestampMs;
          frame->isFullFrame = spd.isFullFrame;
          frame->compressed = false;
          frame->mimeType = spd.mimeType;
          frame->roi_x = spd.roi_x;
          frame->roi_y = spd.roi_y;
          frame->roi_w = spd.roi_w;
          frame->roi_h = spd.roi_h;

          try {
            // 若启用录制，则在 Model 层保存帧到磁盘（优先异步提交到
            // ThreadPool）
            if (m_monitorRes && m_monitorRes->recording.load()) {
              try {
                std::string dir;
                {
                  std::lock_guard<std::mutex> lg(m_monitorRes->recordMutex);
                  dir = m_monitorRes->recordDir.empty()
                            ? "recordings"
                            : m_monitorRes->recordDir;
                }
                std::filesystem::create_directories(dir);
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                std::tm tm{};
#ifdef _WIN32
                localtime_s(&tm, &t);
#else
                localtime_r(&t, &tm);
#endif
                int idx = m_monitorRes->recordFrameIndex.fetch_add(1);
                std::ostringstream ss;
                ss << "frame_" << std::put_time(&tm, "%Y%m%d_%H%M%S_") << idx
                   << ".png";
                std::string filename = ss.str();
                std::string path = (dir + std::string("\\") + filename);

                // 如果存在 ThreadPool，则异步保存
                if (m_screenBuffer && m_screenBuffer->pool) {
                  auto pool = m_screenBuffer->pool;
                  // copy what we need into the task
                  auto rgbaCopy = frame->rgba;
                  int w = frame->width;
                  int h = frame->height;
                  auto compressedPayloadCopy = frame->compressedPayload;
                  bool isCompressed = frame->compressed;
                  std::string p = path;
                  pool->enqueue([rgbaCopy, w, h, compressedPayloadCopy,
                                 isCompressed, p]() {
                    try {
                      if (!isCompressed) {
                        SaveRgbaToPng(rgbaCopy, w, h, p);
                      } else if (!compressedPayloadCopy.empty()) {
                        std::ofstream ofs(p, std::ios::binary);
                        ofs.write(
                            reinterpret_cast<const char *>(
                                compressedPayloadCopy.data()),
                            (std::streamsize)compressedPayloadCopy.size());
                      }
                    } catch (...) {
                    }
                  });
                } else {
                  // 同步保存（回退）
                  if (!frame->compressed) {
                    SaveRgbaToPng(frame->rgba, frame->width, frame->height,
                                  path);
                  } else if (!frame->compressedPayload.empty()) {
                    std::ofstream ofs(path, std::ios::binary);
                    ofs.write(reinterpret_cast<const char *>(
                                  frame->compressedPayload.data()),
                              (std::streamsize)frame->compressedPayload.size());
                  }
                }
              } catch (...) {
              }
            }
            m_monitorRes->frameCb(frame);
          } catch (...) {
          }
        }
      } else {
        // decoding failed: optionally forward compressed payload
        if (m_monitorRes->frameCb) {
          auto frame = std::make_shared<FrameData>();
          frame->width = spd.width;
          frame->height = spd.height;
          frame->timestampMs = spd.timestampMs;
          frame->isFullFrame = spd.isFullFrame;
          frame->compressed = spd.compressed;
          frame->compressedPayload = std::move(spd.compressedPayload);
          frame->mimeType = spd.mimeType;
          try {
            // 录制压缩数据（若启用），优先异步提交到 ThreadPool
            if (m_monitorRes && m_monitorRes->recording.load()) {
              try {
                std::string dir;
                {
                  std::lock_guard<std::mutex> lg(m_monitorRes->recordMutex);
                  dir = m_monitorRes->recordDir.empty()
                            ? "recordings"
                            : m_monitorRes->recordDir;
                }
                std::filesystem::create_directories(dir);
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                std::tm tm{};
#ifdef _WIN32
                localtime_s(&tm, &t);
#else
                localtime_r(&t, &tm);
#endif
                int idx = m_monitorRes->recordFrameIndex.fetch_add(1);
                std::ostringstream ss;
                ss << "frame_" << std::put_time(&tm, "%Y%m%d_%H%M%S_") << idx
                   << ".png";
                std::string filename = ss.str();
                std::string path = (dir + std::string("\\") + filename);

                if (m_screenBuffer && m_screenBuffer->pool) {
                  auto pool = m_screenBuffer->pool;
                  auto compressedCopy = frame->compressedPayload;
                  std::string p = path;
                  pool->enqueue([compressedCopy, p]() {
                    try {
                      if (!compressedCopy.empty()) {
                        std::ofstream ofs(p, std::ios::binary);
                        ofs.write(reinterpret_cast<const char *>(
                                      compressedCopy.data()),
                                  (std::streamsize)compressedCopy.size());
                      }
                    } catch (...) {
                    }
                  });
                } else {
                  if (!frame->compressedPayload.empty()) {
                    std::ofstream ofs(path, std::ios::binary);
                    ofs.write(reinterpret_cast<const char *>(
                                  frame->compressedPayload.data()),
                              (std::streamsize)frame->compressedPayload.size());
                  }
                }
              } catch (...) {
              }
            }
            m_monitorRes->frameCb(frame);
          } catch (...) {
          }
        }
      }

      // Respect target fps if set (simple throttle)
      if (m_monitorRes && m_monitorRes->fps > 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(1000 / m_monitorRes->fps));
      }
    }
  });
}

void MonitorModel::stopCapture() {
  if (!m_monitorRes)
    return;
  m_monitorRes->running.store(false);
  // Wake up any waits
  if (m_netBuffer)
    m_netBuffer->queueCv.notify_all();
  m_monitorRes->screenCv.notify_all();

  if (m_monitorRes->netThread.joinable())
    m_monitorRes->netThread.join();
  if (m_monitorRes->decodeThread.joinable())
    m_monitorRes->decodeThread.join();
}

// IIoModel 委托实现
void MonitorModel::setIoModel(std::shared_ptr<IIoModel> io) {
  if (!m_monitorRes)
    m_monitorRes = std::make_unique<MonitorProtocol::MonitorResources>();
  std::lock_guard<std::mutex> lg(
      m_monitorRes->recordMutex); // reuse mutex for simple protection
  m_monitorRes->ioModel = io;
}

void MonitorModel::setNetworkModel(std::shared_ptr<INetworkModel> net) {
  net_ = std::move(net);
  // Ensure protocol resources exist
  if (!m_monitorRes)
    m_monitorRes = std::make_unique<MonitorProtocol::MonitorResources>();

  // Clear existing buffer
  if (m_netBuffer) {
    std::lock_guard<std::mutex> lk(m_netBuffer->queueMutex);
    m_netBuffer->packetQueue.clear();
  }

  if (net_) {
    // When network receives a packet, push it into this model's buffer and notify
    net_->setOnPacketReceived([this](const Packet &p) {
      if (!m_netBuffer) return;
      {
        std::lock_guard<std::mutex> lk(m_netBuffer->queueMutex);
        m_netBuffer->packetQueue.push_back(p);
        while (m_netBuffer->packetQueue.size() > m_netBuffer->maxQueue)
          m_netBuffer->packetQueue.pop_front();
      }
      m_netBuffer->queueCv.notify_one();
    });

    // On disconnect, wake up waiting threads and stop monitor if necessary
    net_->setOnStatusChanged([this](bool connected) {
      if (!connected) {
        if (m_monitorRes) {
          m_monitorRes->running.store(false);
          if (m_netBuffer) m_netBuffer->queueCv.notify_all();
          m_monitorRes->screenCv.notify_all();
        }
      }
    });
  }
}

void MonitorModel::injectMouse(int x, int y, int button, bool down) {
  auto io =
      m_monitorRes ? m_monitorRes->ioModel.lock() : std::shared_ptr<IIoModel>();
  if (io) {
    try {
      io->injectMouse(x, y, button, down);
    } catch (...) {
    }
  }
}

void MonitorModel::injectKey(int keycode, bool down) {
  auto io =
      m_monitorRes ? m_monitorRes->ioModel.lock() : std::shared_ptr<IIoModel>();
  if (io) {
    try {
      io->injectKey(keycode, down);
    } catch (...) {
    }
  }
}
