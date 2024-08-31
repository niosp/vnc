#include <winsock2.h>
#include <jpeglib.h>
#include <windows.h>
#include <iostream>
#include <vector>

HBITMAP decompress_jpeg_to_bitmap(const std::vector<unsigned char>& jpeg_data, int& width, int& height) {
    jpeg_decompress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    /* input from memory buffer */
    jpeg_mem_src(&cinfo, jpeg_data.data(), jpeg_data.size());
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    width = cinfo.output_width;
    height = cinfo.output_height;
    int row_stride = width * cinfo.output_components;

    std::vector<unsigned char> buffer(row_stride * height);
    while (cinfo.output_scanline < height) {
        unsigned char* row_pointer = buffer.data() + cinfo.output_scanline * row_stride;
        jpeg_read_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    /* create a bitmap from the decompressed data */
    HBITMAP h_bitmap = CreateBitmap(width, height, 1, 24, buffer.data());
    return h_bitmap;
}

std::vector<unsigned char> receive_image(SOCKET server_socket) {
    int data_size;

    /* receive size of  jpeg data */
    recv(server_socket, reinterpret_cast<char*>(&data_size), sizeof(data_size), 0);

    /* receive jpeg data itself*/
    std::vector<unsigned char> jpeg_data(data_size);
    recv(server_socket, reinterpret_cast<char*>(jpeg_data.data()), data_size, 0);

    return jpeg_data;
}

HWND create_window(HINSTANCE h_instance, int width, int height) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = window_proc;
    wc.hInstance = h_instance;
    wc.lpszClassName = "VNCClientWindowClass";
    RegisterClass(&wc);

    /* creates the window (later: contains screen content from vnc server) */
    HWND hwnd = CreateWindow("VNCClientWindowClass", "VNC Client", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, h_instance, NULL);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return hwnd;
}

void display_bitmap(HBITMAP h_bitmap, HWND hwnd) {
    /* get window DC! */
    HDC hdc_window = GetDC(hwnd);
    HDC hdc_mem = CreateCompatibleDC(hdc_window);
    SelectObject(hdc_mem, h_bitmap);

    BITMAP bitmap;
    GetObject(h_bitmap, sizeof(BITMAP), &bitmap);

    /* copy the bitmap to the window */
    BitBlt(hdc_window, 0, 0, bitmap.bmWidth, bitmap.bmHeight, hdc_mem, 0, 0, SRCCOPY);

    ReleaseDC(hwnd, hdc_window);
    DeleteDC(hdc_mem);
}

void send_keystroke(SOCKET client_socket, WPARAM w_param) {
    /* sends the keystrokes to vnc server which then processes them */
    char key = static_cast<char>(w_param);
    send(client_socket, &key, sizeof(key), 0);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT u_msg, WPARAM w_param, LPARAM l_param) {
    SOCKET client_socket;

    switch (u_msg) {
    case WM_KEYDOWN:
        /* send keystroke to server */
        send_keystroke(client_socket, w_param);
        break;
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        /* send mouse events to server */
        send_mouse_event(client_socket, GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param), w_param, u_msg);
        break;
    case WM_CREATE:
        client_socket = *(SOCKET*)((CREATESTRUCT*)l_param)->lpCreateParams;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, u_msg, w_param, l_param);
}

void send_mouse_event(SOCKET client_socket, int x, int y, WPARAM w_param, UINT u_msg) {
    /* mouse event consists of x and y relative coordinates, u_msg and param section */
    struct mouse_event {
        int x;
        int y;
        WPARAM w_param;
        UINT u_msg;
    } event = { x, y, w_param, u_msg };

    /* send the data over the network, data is stuctured like above (struct mouse_event) */
    send(client_socket, reinterpret_cast<char*>(&event), sizeof(event), 0);
}


int main(HINSTANCE h_instance, HINSTANCE, LPSTR, int) {
    /* init winsock */
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);

    /* addr setup */
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* listen for incoming connections (from vnc servers) */
    bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 1);

    /* accept the incoming connection */
    SOCKET client_socket = accept(server_socket, NULL, NULL);

    int width = 800;
    int height = 600;
    HWND hwnd = create_window(h_instance, width, height);

    while (true) {
        /* receive the jpeg image */
        std::vector<unsigned char> jpeg_data = receive_image(client_socket);

        /* decompress jpeg to bitmap */
        HBITMAP h_bitmap = decompress_jpeg_to_bitmap(jpeg_data, width, height);

        SetWindowPos(hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);

        DeleteObject(h_bitmap);

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) break;
        }

        /* window quit message received -> terminate REPL */
        if (msg.message == WM_QUIT) break;
    }

    /* close the sockets, cleanup! */
    closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();

    return 0;
}
