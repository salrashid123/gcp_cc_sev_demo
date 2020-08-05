// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define SET_C_BIT 1
#define CLEAR_C_BIT 0

void print_hex(unsigned char *buf, int len);
int initialize(char **secret);


int main(int argc, char *argv)
{
    int f;
    char *secret = NULL;

    f = open("/dev/sevtest", 0);
    if (f == -1) {
        printf("\nopen /dev/sevtest failed.\n");
        exit(errno);
    }

    if(initialize(&secret))
    {
      close(f);
      exit(0);
    }
    //printf("\r\n secret address is : %lx", (unsigned long)secret);
    printf("\nDumping GCE VM view of the encrypted page...\n");
    printf("(Read access will get decrypted page contents from hardware)\n");
    print_hex(secret, 0xff);

    printf("\nTurning encryption off on the page to simulate host view of the page.\n");
    int ret = ioctl(f,CLEAR_C_BIT,secret);
    if (ret)
        printf("\nioctl failed. check dmesg log for driver errors.\n");

    printf("Dumping the page again...\n");
    printf("(Hardware will not decrypt reads this time because page is not marked encrypted anymore)\n");
    print_hex(secret, 0xff);

    // Re-setting the C bit, just-in-case, I dont know how C bit will be managed once page is freed. 
    ioctl(f,SET_C_BIT,secret);

    free(secret);
    close(f);
}

int initialize(char **secret)
{
    //allocate a page
    int pagesize = 0;
    pagesize = sysconf(_SC_PAGESIZE);
    //printf("The page size is: %d\r\n", pagesize);
    *secret = memalign(pagesize, pagesize); 
    if(!*secret)
    {
        printf("\nmemalign fail.\n");
        return 1;
    }

    memset(*secret, 0, pagesize);
    printf("\nSuccessfully allocated an AMD SEV encrypted page, private to GCE VM.\n");
    printf("(Memory allocated is encrypted by default, nothing special here)\n");
    printf("\nEnter a secret string to write to this encrypted page:");
    fgets(*secret, pagesize, stdin);
    return 0;
}


void print_hex(unsigned char *buf, int len)
{
    int i = 0, j = 0, k = 0;
    for(i = 0; i < len; i++)
    {
        printf("\n%p: ", buf);
        k = i;
        for(j=0; j < 16 && i < len; j++)
        {
           printf("%02X ", buf[i]);
           i++;
        }
        for(;j<16;j++)
        {
            printf("   "); // print space to fill rest of the line
        }

	printf("\t");
        i = k;

        for(j=0; j < 16 && i < len; j++)
        {
           if(buf[i] > 31 && buf[i] !=127)
               printf("%c", buf[i]);
           else
              printf("%c",46);
           i++;
        }
        i--;
    }
   printf("\n");
}