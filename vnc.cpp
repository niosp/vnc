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

    /* copy screen to memory (use BitBlt from WinAPI) */
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
    unsigned char* mem_buf = nullptr;
    unsigned long size = 0;
    jpeg_mem_dest(&cinfo, mem_buf, &size);

    /* set image parameters, set colorspace */
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 75, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    int row_stride = width * 3;
    std::vector<JSAMPLE> image_data(row_stride * height);

    HDC hdc = CreateCompatibleDC(NULL);
    SelectObject(hdc, h_bitmap);
    BITMAPINFOHEADER bmi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 24, BI_RGB, 0, 0, 0, 0, 0 };
    GetDIBits(hdc, h_bitmap, 0, height, image_data.data(), (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
    DeleteDC(hdc);

    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer = &image_data[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    /* finish compression */
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    /* save to vector */
    jpeg_data.assign(mem_buf, mem_buf + size);
    free(mem_buf);

    /* the vector with compressed bitmap of screen contents */
    return jpeg_data;
}

void send_image(SOCKET client_socket, const std::vector<unsigned char>& jpeg_data) {
    int data_size = jpeg_data.size();
    
    /* send size of image first, then the image data */
    send(client_socket, reinterpret_cast<const char*>(&data_size), sizeof(data_size), 0);
    send(client_socket, reinterpret_cast<const char*>(jpeg_data.data()), data_size, 0);
}

void simulate_keystroke(char key) {
    /* prepare WinAPI input types */
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VkKeyScan(key);

    /* send key down */
    SendInput(1, &input, sizeof(INPUT));

    /* key up event */
    input.ki.dwFlags = KEYEVENTF_KEYUP;

    /* send key up (key got released) */
    SendInput(1, &input, sizeof(INPUT));
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

    /* address and port are from argument 1 and 2 */
    server_addr.sin_port = htons(std::stoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    /* connect to the client */
    connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr));

    /* start time: before processing images (used to calculate the fps later on) */
    int frame_count = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (true) {
        int width, height;

        /* capture screen */
        HBITMAP h_bitmap = capture_screen(width, height);

        /* compress the image to jpeg */
        std::vector<unsigned char> jpeg_data = compress_to_jpeg(h_bitmap, width, height);

        /* send jpeg to the client */
        send_image(client_socket, jpeg_data);

        /* check if there's incoming keystrokes (read them from the socket) */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        timeval timeout = { 0, 1000 };

        int result = select(0, &read_fds, NULL, NULL, &timeout);
        if (result > 0 && FD_ISSET(client_socket, &read_fds)) {
            char buffer[sizeof(MouseEvent)];
            recv(client_socket, buffer, sizeof(buffer), 0);

            MouseEvent* event = reinterpret_cast<MouseEvent*>(buffer);

            /* simulate keystroke or mouse event on server */
            if (event->u_msg >= WM_MOUSEFIRST && event->u_msg <= WM_MOUSELAST) {
                simulate_mouse_event(event->x, event->y, event->w_param, event->u_msg);
            }
            else {
                simulate_keystroke(static_cast<char>(event->w_param));
            }
        }

        /* sleep for 10 milliseconds. good practice otherwise the buffers can be flooded */
        Sleep(10);

        /* clean up */
        DeleteObject(h_bitmap);

        frame_count++;

        /* calculate and print fps every 10 seconds (prints to console) */
        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = current_time - start_time;
        if (elapsed.count() >= 10.0) {
            double fps = frame_count / elapsed.count();
            std::cout << "FPS: " << fps << std::endl;

            /* reset counter and timer */
            frame_count = 0;
            start_time = current_time;
        }
    }

    /* free allocated resoures */
    closesocket(client_socket);
    WSACleanup();

    return 0;
}
