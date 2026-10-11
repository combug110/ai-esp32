#ifndef CAMERA_H
#define CAMERA_H

#include <string>

class Camera {
public:
    // 多态基类必须有虚析构：否则通过 Camera* 删除派生对象（如
    // Esp32Camera / EspVideo）会触发 -Werror=delete-non-virtual-dtor，
    // 且行为未定义。
    virtual ~Camera() = default;

    virtual void SetExplainUrl(const std::string& url, const std::string& token) = 0;
    virtual bool Capture() = 0;
    virtual bool SetHMirror(bool enabled) = 0;
    virtual bool SetVFlip(bool enabled) = 0;
    virtual bool SetSwapBytes(bool enabled) { return false; }  // Optional, default no-op
    virtual std::string Explain(const std::string& question) = 0;
};

#endif // CAMERA_H
