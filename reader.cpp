//
// Created by Leoberto Soares on 3/6/19.
//

#include "reader.h"

Reader::Reader() {
    sem_init(&writeSemaphore, 0, 0);
    Initialize();
}

void Reader::Erro(string msg) {
    clog << "Error: " << msg << endl;
    exit(0);
}

void Reader::Initialize() {
    erro = camera.Connect();
    if(erro != PGRERROR_OK) Erro("Could not connect to camera.");

    Format7ImageSettings format;
    Format7PacketInfo packet;
    FC2Config config;

    format.pixelFormat = PIXEL_FORMAT_422YUV8;
    format.width = INPUT_WIDTH;
    format.height = INPUT_HEIGHT;

    erro = camera.SetFormat7Configuration(&format, packet.recommendedBytesPerPacket);
    if(erro != PGRERROR_OK) Erro("Could not set image settings");

    erro = camera.GetConfiguration(&config);
    if(erro != PGRERROR_OK) Erro("Could not get standard configuration");

    config.numBuffers = 10;
    config.highPerformanceRetrieveBuffer = true;

    erro = camera.SetConfiguration(&config);
    if(erro != PGRERROR_OK) Erro("Could not set camera configuration");

    erro = camera.StartCapture();
    if(erro != PGRERROR_OK) Erro("Could not start capture");
}

unsigned int Reader::GetStride() {
    Image img;
    unsigned int r, c, stride;
    PixelFormat fmt;

    erro = camera.RetrieveBuffer(&img);
    if(erro != PGRERROR_OK) Erro("Could not retrieve frame");
    img.GetDimensions(&r, &c, &stride, &fmt);
    return stride;
}

void Reader::Run() {
    while(!finish) {
        if(frames.size() < BUFFER_SIZE) {
            Image img;
            erro = camera.RetrieveBuffer(&img);
            if(erro != PGRERROR_OK) Erro("Could not retrieve frame");
            frames.push(img.GetData());
            sem_post(&writeSemaphore);
        }
    }
}