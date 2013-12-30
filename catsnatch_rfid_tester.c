
#include <stdio.h>
#include <stdlib.h>
#include "catsnatch_rfid.h"


void inner_read_cb(catsnatch_rfid_t *rfid, int incomplete, const char *data)
{
	printf("Reply: %s\n", data);
}

int main(int argc, char **argv)
{
	catsnatch_rfid_t rfidin;
	catsnatch_rfid_context_t ctx;

	catsnatch_rfid_init("Inner", &rfidin, "/dev/ttyUSB0", inner_read_cb);
	catsnatch_rfid_ctx_set_inner(&ctx, &rfidin);

	catsnatch_rfid_open(&rfidin);

	while (1)
	{
		catsnatch_rfid_ctx_service(&ctx);
		sleep(1);
	}

	catsnatch_rfid_destroy(&rfidin);

	return 0;
}



/*
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define MODEM "/dev/ttyUSB0"
#define BAUDRATE B9600    

int main(int argc,char** argv)
{   
    struct termios tio;
    struct termios stdio;
    struct termios old_stdio;
    int tty_fd, flags;
    unsigned char c='D';
    tcgetattr(STDOUT_FILENO,&old_stdio);
    printf("Please start with %s /dev/ttyS1 (for example)\n",argv[0]);
    memset(&stdio,0,sizeof(stdio));
    stdio.c_iflag=0;
    stdio.c_oflag=0;
    stdio.c_cflag=0;
    stdio.c_lflag=0;
    stdio.c_cc[VMIN]=1;
    stdio.c_cc[VTIME]=0;
    tcsetattr(STDOUT_FILENO,TCSANOW,&stdio);
    tcsetattr(STDOUT_FILENO,TCSAFLUSH,&stdio);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);       // make the reads non-blocking
    memset(&tio,0,sizeof(tio));
    tio.c_iflag=0;
    tio.c_oflag=0;
    tio.c_cflag=CS8|CREAD|CLOCAL;           // 8n1, see termios.h for more information
    tio.c_lflag=0;
    tio.c_cc[VMIN]=1;
    tio.c_cc[VTIME]=5;
    if((tty_fd = open(MODEM , O_RDWR | O_NONBLOCK)) == -1){
        printf("Error while opening\n"); // Just if you want user interface error control
        return -1;
    }
    cfsetospeed(&tio,BAUDRATE);    
    cfsetispeed(&tio,BAUDRATE);            // baudrate is declarated above
    tcsetattr(tty_fd,TCSANOW,&tio);
    while (c!='q'){
        if (read(tty_fd,&c,1)>0){
            write(STDOUT_FILENO,&c,1); // if new data is available on the serial port, print it out
            printf("\n");
        }
        if (read(STDIN_FILENO,&c,1)>0){
            write(tty_fd,&c,1);//if new data is available on the console, send it to serial port
            printf("\n");
        }
    }
    close(tty_fd);
    tcsetattr(STDOUT_FILENO,TCSANOW,&old_stdio);
    return EXIT_SUCCESS;
}
*/
