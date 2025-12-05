#pragma once

class ServerRunner {
public:
    explicit ServerRunner(unsigned short port);
    // 返回进程退出码
    int Run();

private:
    unsigned short port_;
};
