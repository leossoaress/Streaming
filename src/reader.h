//
// Created by Leoberto Soares on 3/6/19.
//

#ifndef STREAMING_READER_H
#define STREAMING_READER_H

#include <semaphore.h>
#include <iostream>
#include <string>
#include <queue>

#include <flycapture/FlyCapture2Video.h>
#include <flycapture/FlyCapture2.h>

#define BUFFER_SIZE 16
#define INPUT_WIDTH 4096
#define INPUT_HEIGHT 2160

using namespace FlyCapture2;
using namespace std;

class Reader {

private:

    //Point gray camera class
    Camera camera;

    //Erro type
    Error erro;

    //Boolean to control the end
    bool finish = false;

    //Function to initialize camera
    void Initialize();

    //Function to print erro
    void Erro(string msg);

public:

    //Default constructor
    Reader();

    //Thread function to get frames
    void Run();

    unsigned int GetStride();

    //Queue of frames
    std::queue<uint8_t*> frames;

    //Write semaphore
    sem_t writeSemaphore;
};


#endif //STREAMING_READER_H
