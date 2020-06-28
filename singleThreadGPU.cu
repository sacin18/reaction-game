
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <chrono>
#include <stdio.h>
#include <ole2.h>
#include <olectl.h>
#include <string>
//#include <atlbase.h>
#include <WinUser.h>
#include <iostream>

#define GLOBAL_SIZE 2

#define CAPTURE_WIDTH 871
#define CAPTURE_HEIGHT 747
#define CAPTURE_X_OFFSET 10
#define CAPTURE_Y_OFFSET 40

#define LINE_Y_OFFSET -50.1
#define LINE_X_OFFSET 2

#define TOTAL_THREADS 1

int iterClicked = 0;
int numberOfThreads = 0;
bool threadsInUse = false;
bool saveBitmap(LPCSTR filename, HBITMAP bmp, HPALETTE pal);
void mouseClick(int px, int py);

class pixel {
public:
    unsigned char r, g, b;
    pixel(unsigned char r, unsigned char g, unsigned char b) {
        this->r = r; this->g = g; this->b = b;
    }
    pixel() {}
    std::string disp() {
        return "(" + std::to_string(int(r)) + "," + std::to_string(int(g)) + "," + std::to_string(int(b)) + ")";
    }
};
pixel** buffer;
pixel* buffer1D;
int bufferHeight=0, bufferWidth=0;

__global__ void scanAndClickKernel(pixel* grid, int gridHeight, int gridWidth, int* sol) {
    int offsetX = LINE_X_OFFSET;
    float offsetY = LINE_Y_OFFSET;
    for (int i = 0; i < gridHeight; i++) {
        for (int j = 0; j < gridWidth; j++) {
            if (grid[i * gridWidth + j].r == 255 && grid[i * gridWidth + j].g == 0 && grid[i * gridWidth + j].b == 0) {
                //++iterClicked;
                //if (iterClicked == 1) {
                //mouseClick(int(float(j + offsetX + CAPTURE_X_OFFSET) / 1920 * 65535), int(float(gridHeight - (i + offsetY + CAPTURE_Y_OFFSET)) / 1080 * 65535));
                //}
                sol[0] = j; sol[1] = i;
                return;
            }
        }
    }
}

cudaError_t targetWithCuda(pixel* grid, unsigned int height, unsigned int width, int* sol);

unsigned char* readBitmapFile(std::string filename, int padding = 1) {
    FILE* f;
    fopen_s(&f, filename.c_str(), "rb");
    unsigned char info[54];

    // read the 54-byte header
    fread(info, sizeof(unsigned char), 54, f);

    // extract image height and width from header
    int width = *(int*)&info[18];
    int height = *(int*)&info[22];
    bufferHeight = width; bufferHeight = height;
    buffer = new pixel*[height];
    for (int i = 0; i < height;i++) {
        buffer[i] = new pixel[width];
    }// std::vector<std::vector<pixel>>(height, std::vector<pixel>(width));

    // read the rest of the data at once
    unsigned char* data = new unsigned char[width * 3 + padding];
    for (int i = 0; i < height; i++) {
        fread(data, sizeof(unsigned char), width * 3 + padding, f);
        for (int j = 0; j < width * 3; j += 3) {
            buffer[i][j / 3] = pixel(data[j + 2], data[j + 1], data[j]);
        }
    }
    fclose(f);

    return data;
}

void readBitmapFileAs1DArray(std::string filename, int padding = 1) {
    FILE* f;
    fopen_s(&f, filename.c_str(), "rb");
    unsigned char info[54];

    // read the 54-byte header
    fread(info, sizeof(unsigned char), 54, f);

    // extract image height and width from header
    int width = *(int*)&info[18];
    int height = *(int*)&info[22];
    bufferWidth = width; bufferHeight = height;
    buffer1D = new pixel[height*width];
    int size = 3 * width * height;

    // read the rest of the data at once
    unsigned char* data = new unsigned char[width * 3 + padding];
    for (int i = 0; i < height; i++) {
        fread(data, sizeof(unsigned char), width * 3 + padding, f);
        for (int j = 0; j < width * 3; j += 3) {
            buffer1D[i*width + j / 3] = pixel(data[j + 2], data[j + 1], data[j]);
        }
    }
    fclose(f);

    return;
}

bool screenCapturePart(int x, int y, int w, int h, LPCSTR fname) {
    HDC hdcSource = GetDC(NULL);
    HDC hdcMemory = CreateCompatibleDC(hdcSource);

    int capX = GetDeviceCaps(hdcSource, HORZRES);
    int capY = GetDeviceCaps(hdcSource, VERTRES);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcSource, w, h);
    HBITMAP hBitmapOld = (HBITMAP)SelectObject(hdcMemory, hBitmap);

    BitBlt(hdcMemory, 0, 0, w, h, hdcSource, x, y, SRCCOPY);
    hBitmap = (HBITMAP)SelectObject(hdcMemory, hBitmapOld);

    DeleteDC(hdcSource);
    DeleteDC(hdcMemory);

    HPALETTE hpal = NULL;
    if (saveBitmap(fname, hBitmap, hpal)) return true;
    return false;
}

bool saveBitmap(LPCSTR filename, HBITMAP bmp, HPALETTE pal) {
    bool result = false;
    PICTDESC pd;

    pd.cbSizeofstruct = sizeof(PICTDESC);
    pd.picType = PICTYPE_BITMAP;
    pd.bmp.hbitmap = bmp;
    pd.bmp.hpal = pal;

    LPPICTURE picture;
    HRESULT res = OleCreatePictureIndirect(&pd, IID_IPicture, false,
        reinterpret_cast<void**>(&picture));

    if (!SUCCEEDED(res))
        return false;

    LPSTREAM stream;
    res = CreateStreamOnHGlobal(0, true, &stream);

    if (!SUCCEEDED(res)) {
        picture->Release();
        return false;
    }


    LONG bytes_streamed;
    res = picture->SaveAsFile(stream, true, &bytes_streamed);

    //USES_CONVERSION;
    HANDLE file = CreateFile((filename), GENERIC_WRITE, FILE_SHARE_READ, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (!SUCCEEDED(res) || !file) {
        stream->Release();
        picture->Release();
        return false;
    }

    HGLOBAL mem = 0;
    GetHGlobalFromStream(stream, &mem);
    LPVOID data = GlobalLock(mem);

    DWORD bytes_written;

    result = !!WriteFile(file, data, bytes_streamed, &bytes_written, 0);
    result &= (bytes_written == static_cast<DWORD>(bytes_streamed));

    GlobalUnlock(mem);
    CloseHandle(file);

    stream->Release();
    picture->Release();

    return result;
}

void mouseClick(int px, int py) {
    INPUT Inputs[3] = { 0 };

    Inputs[0].type = INPUT_MOUSE;
    Inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
    Inputs[0].mi.dx = px; // desired X coordinate
    Inputs[0].mi.dy = py; // desired Y coordinate

    Inputs[1].type = INPUT_MOUSE;
    Inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    Inputs[2].type = INPUT_MOUSE;
    Inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    std::cout << "click at " << "(" << px << ", " << py << ")" << std::endl;
    SendInput(3, Inputs, sizeof(INPUT));
}

void threadFunction(int id, int len, int totalRows, int totalThreads, int iter) {
    int sharestart = id * totalRows / totalThreads;
    int shareend = min(id * totalRows / totalThreads + (totalRows / totalThreads) - 1, totalRows - 1);
    for (int i = sharestart; i <= shareend; i++) {
        if (iterClicked > 0)return;
        for (int j = 0; j < len; j++) {
            if (iterClicked > 0)return;
            if (buffer[i][j].r == 255 && buffer[i][j].g == 0 && buffer[i][j].b == 0) {
                ++iterClicked;
                if (iterClicked == 1) {
                    //mouseClick(int(float(j + offsetX + CAPTURE_X_OFFSET) / 1920 * 65535), int(float(totalRows - (i + offsetY + CAPTURE_Y_OFFSET)) / 1080 * 65535));
                    std::cout << j <<"-" << i << std::endl;
                }
                return;
            }
        }
    }
}

int main() {
    /*
    const int arraySize = GLOBAL_SIZE;
    int a[arraySize] = { 1, 2 };// , 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    int b[arraySize] = { 10, 20 };// , 30, 40, 50, 10, 20, 30, 40, 50, 10, 20, 30, 40, 50, 10, 20, 30, 40, 50, 10, 20, 30, 40, 50, 10, 20, 30, 40, 50, 10, 20, 30, 40, 50, 10, 20, 30, 40, 50};
    int c[arraySize] = { 0 };
    int d[arraySize] = { 0 };

    auto start = std::chrono::high_resolution_clock::now();
    // Add vectors in parallel.
    cudaError_t cudaStatus = addWithCuda(c, a, b, arraySize);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "addWithCuda failed!");
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "time taken by GPU : " << duration.count() << std::endl;

    start = std::chrono::high_resolution_clock::now();
    addCPU(d, a, b, arraySize);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "time taken by CPU : " << duration.count() << std::endl;

    printf("{1,2,3,4,5} + {10,20,30,40,50} = {%d,%d,%d,%d,%d}\n",
        c[0], c[1], c[2], c[3], c[4]);

    // cudaDeviceReset must be called before exiting in order for profiling and
    // tracing tools such as Nsight and Visual Profiler to show complete traces.
    cudaStatus = cudaDeviceReset();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaDeviceReset failed!");
        return 1;
    }
    */

    mouseClick(5000, 700);
    Sleep(1000);
    //mouseClick(int(float(10)/1920*65535), int(float(39) / 1080 * 65535));
    //Sleep(20);
    //mouseClick(int(float(871) / 1920 * 65535), int(float(787) / 1080 * 65535));

    int offsetX = LINE_X_OFFSET;
    float offsetY = LINE_Y_OFFSET;

    std::string filename = "screenshots/multithreadcup";
    //std::thread threadRow[TOTAL_THREADS];
    for (int iter = 1; iter <= 11; iter++) {
        screenCapturePart(CAPTURE_X_OFFSET, CAPTURE_Y_OFFSET, CAPTURE_WIDTH, CAPTURE_HEIGHT, (filename + std::to_string(iter) + ".bmp").c_str());
        readBitmapFileAs1DArray((filename + std::to_string(iter) + ".bmp"), 3);
        int* sol=new int[2];
        
        cudaError_t cudaStatus = targetWithCuda(buffer1D, bufferHeight, bufferWidth, sol);
        if (cudaStatus != cudaSuccess) {
            fprintf(stderr, "addWithCuda failed!");
            return 1;
        }

        mouseClick(int(float(sol[0] + offsetX + CAPTURE_X_OFFSET) / 1920 * 65535), int(float(bufferHeight - (sol[1] + offsetY + CAPTURE_Y_OFFSET)) / 1080 * 65535));

        //avgX /= avgCounter; avgY /= avgCounter;
        //avgY = buffer.size() - avgY;
        //std::cout << avgX << " " << avgY << std::endl;
        //mouseClick(int(float(avgX + 10) / 1920 * 65535), int(float(avgY + 40) / 1080 * 65535));
    }

    return 0;
}

cudaError_t targetWithCuda(pixel* grid, unsigned int height, unsigned int width, int *sol) {
    int* cudaSol = 0;
    pixel* cudaGrid = 0;
    cudaError_t cudaStatus;
    unsigned int size = 1;

    // Choose which GPU to run on, change this on a multi-GPU system.
    cudaStatus = cudaSetDevice(0);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaSetDevice failed!  Do you have a CUDA-capable GPU installed?");
        goto Error;
    }

    // Allocate GPU buffers for three vectors (two input, one output).
    cudaStatus = cudaMalloc((void**)&cudaGrid, height * width * sizeof(pixel));
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaMalloc failed!");
        goto Error;
    }

    cudaStatus = cudaMalloc((void**)&cudaSol, 2 * sizeof(int));
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaMalloc failed!");
        goto Error;
    }

    cudaStatus = cudaMemcpy(cudaGrid, grid, height * width * sizeof(pixel), cudaMemcpyHostToDevice);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaMemcpy failed!");
        goto Error;
    }

    cudaStatus = cudaMemcpy(cudaSol, sol, 2 * sizeof(int), cudaMemcpyHostToDevice);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaMemcpy failed!");
        goto Error;
    }

    // Launch a kernel on the GPU with one thread for each element.
    scanAndClickKernel <<<1, size >>> (cudaGrid, height, width, cudaSol);
    //auto end = std::chrono::high_resolution_clock::now();
    //auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    //std::cout << "time taken by GPU : " << duration.count() << std::endl;

    // Check for any errors launching the kernel
    cudaStatus = cudaGetLastError();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "addKernel launch failed: %s\n", cudaGetErrorString(cudaStatus));
        goto Error;
    }

    // cudaDeviceSynchronize waits for the kernel to finish, and returns
    // any errors encountered during the launch.
    cudaStatus = cudaDeviceSynchronize();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching addKernel!\n", cudaStatus);
        goto Error;
    }

    // Copy output vector from GPU buffer to host memory.
    
    cudaStatus = cudaMemcpy(sol, cudaSol, 2 * sizeof(int), cudaMemcpyDeviceToHost);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaMemcpy failed!");
        goto Error;
    }
    

    Error:
    cudaFree(cudaGrid);

    return cudaStatus;
}
