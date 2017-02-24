//includes from linux syscalls used from given syscalls
#include "iso_font.h"
#include <sys/types.h>
#include <time.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <termios.h>
#include <sys/stat.h>

typedef unsigned short colour;
//global variable
int file; //used for file opening as well as in the ioctyl
unsigned long screenSize;
unsigned short *fbp; //for our mutex err mmap
unsigned long xAxis; //holds our width of our window for all processes to see
unsigned long yAxis; //holds our hieght for all of out processes to see


void init_graphics(){
	
	struct fb_var_screeninfo vinfo;
    	struct fb_fix_screeninfo finfo;
	struct termios term;

	//char *fbp = 0; moved to a global variable as a short

	file = open("/dev/fb0", O_RDWR);
	/*
	
	if(file == -1)
	{
		perror("Error: cannot open framebuffer device");
		exit(1);
	}
	*/

	// Get fixed screen information
	/*
    	if (ioctl(file, FBIOGET_FSCREENINFO, &finfo) == -1) {
        	perror("Error reading fixed information");
        	exit(2);
    	}
	*/
	
	ioctl(file, FBIOGET_FSCREENINFO, &finfo);
	
    	// Get variable screen information
	/*
    	if (ioctl(file, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        	perror("Error reading variable information");
        	exit(3);
    	}
	*/

	ioctl(file, FBIOGET_VSCREENINFO, &vinfo);

    	//printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel); cant use c libraries

	xAxis = vinfo.xres_virtual;
	yAxis = vinfo.yres_virtual;

	// Figure out the size of the screen in bytes
    	//screenSize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	//screenSize = finfo.size;
	screenSize = finfo.line_length;

    	// Map the device to memory
	// equivalent to mutex as learned from recitation
    	fbp = (unsigned short *)mmap(NULL, yAxis * screenSize, PROT_WRITE, MAP_SHARED, file, 0);

	/*
	if (fbp == -1) {
        	perror("Error: failed to map framebuffer device to memory");
        	exit(4);
    	}
	*/

	ioctl(STDIN_FILENO, TCGETS, &term);

	term.c_lflag &= ~ICANON;
	term.c_lflag &= ~ECHO;
	
	ioctl(STDIN_FILENO, TCSETS, &term);
	clear_screen();

}

void exit_graphics(){

	struct termios term;

	//ioctl to re-enable user input
	ioctl( STDIN_FILENO, TCGETS, &term );
	
	term.c_lflag |= ECHO;
	term.c_lflag |= ICANON;
	ioctl( STDIN_FILENO, TCSETS, &term );

	//munmap() which is free() to the mutex
	munmap( fbp, screenSize * yAxis ); 

	//close()
	close(file);


}

void clear_screen(){
//ansi code to clear screen
//"\033[2j"
	int clear = write(1,"\033[2J", 8);

}

char getkey(){
//read system call from user to make a game
//actually the instructions lie and then tell you to use 
//select as it is non blocking
	fd_set fd;
	struct timeval time;

	int temp1;
	char temp2;

	FD_ZERO(&fd);
	FD_SET(STDIN_FILENO, &fd);

	time.tv_sec = 5;
	time.tv_usec = 0;

	//select as said earlier
	temp1 = select(STDIN_FILENO+1, &fd, NULL, NULL, &time);	

	if(temp1 > 0){
		read(0, &temp2, sizeof(temp2));
	}

	return temp2;	
}

void sleep_ms(long ms){
//makes program sleep in between frames
//with the use of nanosleep
	struct timespec time;
	time.tv_sec = 0;

	//need to user input multilied with one million
	time.tv_nsec = ms * 1000000;

	nanosleep(&time, NULL);
}

void draw_pixel(int x, int y, colour color){
//how the user can draw starting 
//at the input of x and y coordinates
	
	if(x < 0 || x >= xAxis){

		if(y < 0 || y >= yAxis){

		return;

		}

	}
 	unsigned long yAxis1 = (screenSize/2) * y;
	unsigned long xAxis1 = x;
	unsigned short *draw_ptr = (fbp + yAxis1 + xAxis1);
	*draw_ptr = color;	

}

void draw_rect(int x1, int y1, int width, int height, colour color){
//creating a rectangle with a corner at 
//coordinates x and y, adding the width and heights to 
//solve the other corners
	int x, y;
   	for (x = x1; x < x1+width; x++){
      		for (y = y1; y < y1+height; y++){
         		if (x == x1 || x == x1+width-1){
            			draw_pixel(x, y, color);
         		}
         		if (y == y1 || y == y1+height-1){
            			draw_pixel(x, y, color);
         		}
      		}
   	} 
} 

void fill_rect(int x1, int y1, int width, int height, colour color){
//same as draw_rect but the 
//rectangle is a solid color
//rather than an outline
	int x;
	int y;	
	for(x = x1; x < x1 + width; x++){
		for(y = y1; y < y1 + height; y++){
			draw_pixel(x, y, color);
		}
	}
}

void draw_text(int x, int y, const char *text, colour color){
	const char *temp_ptr;
   	int offset = 0;

   	for (temp_ptr = text; *temp_ptr != '\0'; temp_ptr++){

      		draw_character(x, y+offset, color, *temp_ptr);
      		offset += 8;
  	}
}

//easier to split text into two since text is bascially built fomr the characters
//also hinted from misurda project page
void draw_character(int x, int y, colour c, int ascii){

   int i, j, b;

   for (i = 0; i < 16; i++){

      for (j = 0; j < 16; j++){

     	 b =  ((iso_font[ascii * 16+i] & 1 << j) >> j);

         if (b == 1){
            draw_pixel(y+j, x+i, c);
         }
      }
   }
}