#include <stdio.h>

typedef unsigned short colour; 
void init_graphics();
void clear_screen(); 
void exit_graphics(); 
char getkey(); 
void sleep_ms(long ms); 
void draw_pixel(int x, int y, colour color); 
void draw_rect(int x1, int y1, int width, int height, colour c);
void fill_rect(int x1, int y1, int width, int height, colour color);
void draw_text(int x, int y, const char *text, colour color);
void draw_character(int x, int y, colour c, int ascii);
 
int main(int argc, char** argv) 
{ 
	printf("1: draw_rect\n");
   	printf("2: fill_rect\n");
   	printf("3: draw_text\n");

   	char key;
   	int x = (620)/2;
   	int y = (460)/2;
   	int choice;
   	scanf("%d", &choice);

   	if(choice == 1){
      		init_graphics();
      		clear_screen();
      		draw_rect(x, y, 200, 100, 20);
      		do{
        		key = getkey();
         		if(key == 'w'){
				y-=10;
			}
         		else if(key == 's') y+=10;
         		else if(key == 'a') x-=10;
         		else if(key == 'd') x+=10;
         		clear_screen();
         		draw_rect(x, y, 200, 100, 20);
         		sleep_ms(20);
      		}while(key != 'q');
      		clear_screen();
      		exit_graphics();
   	}
	if(choice == 2){
      		init_graphics();
      		clear_screen();
      		fill_rect(x, y, 200, 100, 2);
      		do{
         		key = getkey();
         		if(key == 'w') y-=10;
         		else if(key == 's') y+=10;
         		else if(key == 'a') x-=10;
         		else if(key == 'd') x+=10;
         		clear_screen();
         		fill_rect(x, y, 200, 100, 20);
         		sleep_ms(20);
      		}while(key != 'q');
         	clear_screen();
         	exit_graphics();
   	}

  
   	if(choice == 3){
      		const char *text_input = "Hello World!";
      		init_graphics();
      		clear_screen();
      		draw_text(x, y, text_input, 20);
   		do{
      			key = getkey();
      			if(key == 'w') x-=10;
      			else if(key == 's') x+=10;
      			else if(key == 'a') y-=10;
      			else if(key == 'd') y+=10;
      			clear_screen();
      			draw_text(x, y, text_input, 20);
      			sleep_ms(20);
   		}while(key != 'q');
      		clear_screen();
      		exit_graphics();
   
	}
   	return 0;
}