#include <windows.h>
#include <winsock2.h>
#include <jpeglib.h>
#include <iostream>
#include <vector>

HBITMAP capture_screen(int& width, int& height) {
    HDC hdc_screen = GetDC(NULL);
    HDC hdc_mem = CreateCompatibleDC(hdc_screen);

    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP h_bitmap = CreateCompatibleBitmap(hdc_screen, width, height);
    SelectObject(hdc_mem, h_bitmap);

    /* copy screen to memory */
    BitBlt(hdc_mem, 0, 0, width, height, hdc_screen, 0, 0, SRCCOPY);

    DeleteDC(hdc_mem);
    ReleaseDC(NULL, hdc_screen);

    return h_bitmap;
}

std::vector<unsigned char> compress_to_jpeg(HBITMAP h_bitmap, int width, int height) {
    BITMAP bitmap;
    GetObject(h_bitmap, sizeof(BITMAP), &bitmap);

    std::vector<unsigned char> jpeg_data;
    jpeg_data.reserve(width * height * 3);

    /* set up jpeg compression (done with libjpeg) */
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    /* output to memory buffer */
    unsigned char* buffer = nullptr;
    unsigned long size = 0;
    jpeg_mem_dest(&cinfo, &buffer, &size);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 75, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    int row_stride = width * 3;
    std::vector<JSAMPLE> image_data(row_stride * height);

    /* save to vector */
    jpeg_data.assign(buffer, buffer + size);
    free(buffer);

    return jpeg_data;
}

void send_image(SOCKET client_socket, const std::vector<unsigned char>& jpeg_data) {
    /* todo: work in progress! */
}

int main() {
    /* init winsock! */
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);

    /* create socket */
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);

    /* server address setup */
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080); // todo: get port from argv
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* connect to the client */
    connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr));

    while (true) {
        int width, height;

        /* capture screen */
        HBITMAP h_bitmap = capture_screen(width, height);

        /* compress the image to jpeg */
        std::vector<unsigned char> jpeg_data = compress_to_jpeg(h_bitmap, width, height);

        /* send jpeg to the client */
        send_image(client_socket, jpeg_data);

        /* clean up */
        DeleteObject(h_bitmap);

        Sleep(1000);
    }

    /* free allocated resoures */
    closesocket(client_socket);
    WSACleanup();

    return 0;
}
