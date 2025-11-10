#ifndef _APPSHADERLINUXOGL1_HPP_
#define _APPSHADERLINUXOGL1_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

struct AppShaderLinuxOGL1
{
    struct Opaque;
    Opaque *pOpaque;


    void Bind();
    void Release();

    void BeginFrame();
    void EndFrame();
};

#endif // _APPSHADERLINUXOGL1_HPP_
