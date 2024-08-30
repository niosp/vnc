#include <winsock2.h>
#include <jpeglib.h>
#include <windows.h>
#include <iostream>
#include <vector>

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
    wc.lpfnWndProc = DefWindowProc;
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
        /* receive a jpeg image */
        std::vector<unsigned char> jpeg_data = receive_image(client_socket);

        /* decompress jpeg to bitmap */
        // todo: !
    }

    /* close the sockets, cleanup! */
    closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();

    return 0;
}
