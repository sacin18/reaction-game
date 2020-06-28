
#include <iostream>
#include <ole2.h>
#include <olectl.h>
#include <string>
#include <atlbase.h>
#include <WinUser.h>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

//std::mutex mtx;
//std::condition_variable cv;

#define CAPTURE_WIDTH 871
#define CAPTURE_HEIGHT 747
#define CAPTURE_X_OFFSET 10
#define CAPTURE_Y_OFFSET 40

#define LINE_Y_OFFSET -50.1
#define LINE_X_OFFSET 2

#define TOTAL_THREADS 4

std::atomic<int> iterClicked = 0;
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
std::vector<std::vector<pixel>> buffer;// = std::vector<std::vector<pixel>>();

unsigned char* readBitmapFile(std::string filename, int padding = 1) {
    FILE* f;
    fopen_s(&f, filename.c_str(), "rb");
    unsigned char info[54];

    // read the 54-byte header
    fread(info, sizeof(unsigned char), 54, f);

    // extract image height and width from header
    int width = *(int*)&info[18];
    int height = *(int*)&info[22];
    //std::cout << width << " " << height << std::endl;
    buffer = std::vector<std::vector<pixel>>(height, std::vector<pixel>(width));
    int size = 3 * width * height;
    //unsigned char* data = new unsigned char[size];

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

pixel getPixelFromBitmapBuffer(int x, int y, unsigned int dimX, unsigned int dimY) {
    //assuming your x/y starts from top left, like I usually do
    return buffer[y][x];
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

    USES_CONVERSION;
    HANDLE file = CreateFile(A2W(filename), GENERIC_WRITE, FILE_SHARE_READ, 0,
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

    //SetCursorPos(px, py);
    std::cout << "click at " << "(" << px << ", " << py << ")" << std::endl;
    SendInput(3, Inputs, sizeof(INPUT));
}

void threadFunction(int id,int len,int totalRows,int totalThreads,int iter) {
    //++numberOfThreads;
    //std::cout << number << std::endl;
    int offsetX = LINE_X_OFFSET;
    float offsetY = LINE_Y_OFFSET;
    int sharestart = id * totalRows / totalThreads;
    int shareend = min(id * totalRows / totalThreads + (totalRows / totalThreads) - 1, totalRows - 1);
    for (int i = sharestart; i <= shareend; i++) {
        if (iterClicked > 0)return;
        for (int j = 0; j < len; j++) {
            if (iterClicked > 0)return;
            if (buffer[i][j].r == 255 && buffer[i][j].g == 0 && buffer[i][j].b == 0) {
                //std::cout << "equal" << std::endl;
                ++iterClicked;
                if (iterClicked == 1) {
                    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    mouseClick(int(float(j + offsetX + CAPTURE_X_OFFSET) / 1920 * 65535), int(float(totalRows - (i + offsetY + CAPTURE_Y_OFFSET)) / 1080 * 65535));
                    std::cout << iter << std::endl;
                    //while (numberOfThreads > 1);
                    //threadsInUse = false;
                    //lck.unlock();
                    //cv.notify_all();
                }
                return;
            }
        }
    }
    //if(numberOfThreads>0)--numberOfThreads;
}

int main() {

    mouseClick(5000, 700);
    Sleep(1000);
    //mouseClick(int(float(10)/1920*65535), int(float(39) / 1080 * 65535));
    //Sleep(20);
    //mouseClick(int(float(871) / 1920 * 65535), int(float(787) / 1080 * 65535));

    std::string filename = "screenshots/multithreadcup";
    std::thread threadRow[TOTAL_THREADS];
    for (int iter = 1; iter <= 11; iter++) {
        //std::unique_lock<std::mutex> lck(mtx);
        //lck.lock();
        screenCapturePart(CAPTURE_X_OFFSET, CAPTURE_Y_OFFSET, CAPTURE_WIDTH, CAPTURE_HEIGHT, (filename + std::to_string(iter) + ".bmp").c_str());
        unsigned char* val = readBitmapFile((filename + std::to_string(iter) + ".bmp"), 3);
        int len = buffer[0].size();
        int totalRows = buffer.size();
        threadsInUse = true;
        for (int i = 0; i < TOTAL_THREADS; i++) {
            try {
                threadRow[i] = std::thread(threadFunction, i, len, totalRows, TOTAL_THREADS, iter);
                //threadRow[i].join();
                //threadRow[i].detach();
            } catch (char* excp) {
                std::cout << excp << std::endl;
            }
        }
        //
        for (int i = 0; i < TOTAL_THREADS; i++) {
            if(threadRow[i].joinable())threadRow[i].join();
        }
        //
        //while (threadsInUse);
        //cv.wait(lck);
        iterClicked = 0;
        //avgX /= avgCounter; avgY /= avgCounter;
        //avgY = buffer.size() - avgY;
        //std::cout << avgX << " " << avgY << std::endl;
        //mouseClick(int(float(avgX + 10) / 1920 * 65535), int(float(avgY + 40) / 1080 * 65535));
    }
    return 0;
}
