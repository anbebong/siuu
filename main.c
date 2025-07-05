#include "sse_server.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main() {
    int port = 8080;
    start_sse_server(port);
    // // Server chỉ nhận message từ các chương trình khác qua Unix Domain Socket
    // // Không nhập từ bàn phím nữa
    // while (1) {
    //     pause(); // Chờ tín hiệu, giữ chương trình chạy mãi
    // }

    char msg[256];
    while (1) {
        printf("Nhập nội dung gửi cho client (hoặc Ctrl+C để thoát): ");
        if (fgets(msg, sizeof(msg), stdin) == NULL) break;
        size_t len = strlen(msg);
        if (len > 0 && msg[len-1] == '\n') msg[len-1] = '\0';
        sse_send_message(msg);
    }
    return 0;
}