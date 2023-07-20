#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>
#include <signal.h>

const char *progname;

#define SEND_RADIO_FLAG			0x0001
#define SEND_ANY_FLAG			0x0002
#define SEND_BG_FLAG			0x0004
#define SEND_LEN_FLAG			0x0008
#define SEND_COUNT_FLAG			0x0010
#define SEND_INTERVAL_FLAG			0x0020
#define SEND_FILE_NAME_FLAG			0x0040
#define SEND_TARGET_UTIL_FLAG		0x0080
#define SEND_QUIET_FLAG				0x0100
#define SEND_HELP_FLAG				0x0200

#define BUF_SIZE				128


pthread_t ntid;
const char options[] =
"Options:\n\
-r, --radio (interface, 0:wifi0, 1:wifi1, 2:wifi2)\n\
-a, --any_send (send and frames that -f specific)\n\
-b, --bg_traffic (mode set specific back ground channel utilication)\n\
-l, --len (bg_mode pkt start length)\n\
-c, --count (pkt count per send)\n\
-i, --interval (pkt send interval default:5ms)\n\
-f, --file_name (pkt hex frame data)\n\
-t, --target_util (the target channel util wanted)\n\
-q, --quiet (does not read the bg log)\n\
-h, --help \n\
	";

static void
usage(void)
{
    fprintf(stderr, "Usage:\n%s options\n", progname);
    fprintf(stderr, "%s\n", options);
    exit(-1);
}

#define FILE_NAME_MAX   128
#define FILE_NAME_PRE   "/proc/net/wifi"
#define BG_TRAFFIC      "bg_traffic"
#define FILE_SIZE		4000
#define ANY_FRAME       "any_frame"

int
sysctl_write_param(char *nodename, char *value)
{
     int fd, rc ;

     value = (value == NULL ? "" : value);

     if ((fd = open(nodename, O_RDWR, 0)) < 0) {
          printf("Error opening %s : %s. %s",
                  nodename, strerror(errno), value);
          return -1 ;
     }
     rc = write(fd, value, strlen(value));
     if (rc < 0 && errno != 0) {
          printf("writing %s : %s",
                  nodename, strerror(errno));
          if (errno == ENOMEM)
              rc = ENOMEM;
          else
             rc = -1 ;
     }
     close(fd) ;

     return rc ;
}

int
sysctl_read_param(char *nodename, char *value, int size)
{
     int fd, rc ;

     if ((fd = open(nodename, O_RDONLY, 0)) < 0) {
          printf("Error opening %s : %s",
                  nodename, strerror(errno));
          return -1 ;
     }
     rc = read(fd, value, size);
     if (rc <= 0) {
          printf("reading %s : %s",
                  nodename, strerror(errno));
          rc = -1 ;
     }
     close(fd) ;
     return rc ;
}

void send_bg_traffic(unsigned int radio, int target)
{
    int len;
	int rc;
	char cmdline[BUF_SIZE];
	

    char filename[FILE_NAME_MAX];
    
    len = sprintf(filename, FILE_NAME_PRE "%d" "/" "%s",radio,BG_TRAFFIC);

	snprintf(cmdline, BUF_SIZE, "%d", target);

	rc = sysctl_write_param(filename,cmdline);   

	//printf("sysctl_write_param ret %d\n",rc);

}

void * read_bg_traffic_result(void * radio)
{
	int len;
	int rc;
	char filename[FILE_NAME_MAX];
	char result[BUF_SIZE];
	len = sprintf(filename, FILE_NAME_PRE "%d" "/" "%s",*(unsigned int*)radio,BG_TRAFFIC);
	
	while(1)
	{
		rc = sysctl_read_param(filename,result,BUF_SIZE);
		printf("%s\n",result);
		sleep(1);
	}
}

char assic_to_byte(char * tmp)
{
    char a;

    if(tmp[0] < 0x60)
        a = (tmp[0] - 0x30) << 4; 
    else
        a = (tmp[0] - 0x57) << 4;
    if(tmp[1] < 0x60)
        a |= (tmp[1] - 0x30);
    else
        a |= (tmp[1] - 0x57);
    return a;
}

void send_buf_to_driver(unsigned int radio, char * buf, unsigned int size)
{
    int len;
	char cmdline[BUF_SIZE];
    int fd, rc ;
	

    char filename[FILE_NAME_MAX];
    
    len = sprintf(filename, FILE_NAME_PRE "%d" "/" "%s",radio,ANY_FRAME);

    if ((fd = open(filename, O_RDWR, 0)) < 0) {
         printf("Error opening %s : %s",
                 filename, strerror(errno));
         return ;
    }
    rc = write(fd, buf, size);
    if (rc < 0 && errno != 0) {
         printf("writing %s : %s",
                 filename, strerror(errno));
         if (errno == ENOMEM)
             rc = ENOMEM;
         else
            rc = -1 ;
    }
    close(fd) ;

    if(rc > 0)
	    printf("send %d success\n",rc);
    return ;

    
}

void read_file_and_send(unsigned int radio, char * filename)
{
	int ret = 1,fd;
	char * file_buf;
	int i = 0,j = 0;
    char tmp[3];
    char test;

	file_buf = (char *)malloc(FILE_SIZE);

    if ((fd = open(filename, O_RDONLY, 0)) < 0) {
         printf("Error opening %s : %s",
                 filename, strerror(errno));
         return ;
    }

    while(ret > 0)
    {
        ret = read(fd, &tmp, 3);

        if (ret <= 0) {
        //    printf("\n");
            printf("reading %s : %s\n",
                filename, strerror(errno));
            ret = -1 ;
            break;
        }

        test = assic_to_byte(tmp);
        file_buf[j++] = test;
    }
    /*
	for(i = 0; i< j; i++)
	{
        if((i%16 == 0))
            printf("\n");
        //printf(" %#x",file_buf[i]);
        printf(" %#02x",file_buf[i]);
	}
    */
    printf("\n");
	
    close(fd) ;

    send_buf_to_driver(radio,file_buf,j);
}
unsigned int radio;
void quit_thread(int signo)
{
	send_bg_traffic(radio,1);
	exit(0);
}

void main (int32_t argc, char *argv[])
{
    int32_t c;
    int options = 0;
    char * filename;
	int opt_flag = 0;
    unsigned int pktlen, pkt_count, interval, target;
    int32_t option_index = 0;

	progname = argv[0];

    static struct option long_options[] = {
        {"radio", 0, NULL, 'r'},
        {"any_mode", 0, NULL, 'a'},
        {"bg_mode", 0, NULL, 'b'},
        /* this is for bg_mode */
        {"pktlen", 0, NULL, 'l'},
        {"pkt_count", 0, NULL, 'c'},
        {"interval", 0, NULL, 'i'},
        /* this is for any mode */
        {"file_name", 0, NULL, 'f'},
        {"target_util", 0, NULL, 't'},
		{"quiet",0, NULL, 'q'},
        { 0, 0, 0, 0}
    };
    while (1) {
        c = getopt_long (argc, argv, "r:t:abl::c::i:f:qh", long_options,
                         &option_index);
        if (c == -1) break;
        switch (c) {
            case 'r':
				if(optarg != NULL)
				    radio = (unsigned int)strtol(optarg, (char **)NULL, 10);
				opt_flag |= SEND_RADIO_FLAG;
				//printf("radio is %d\n",radio);
                break;
            case 'a':
				opt_flag |= SEND_ANY_FLAG;
				break;
            case 'b':
				opt_flag |= SEND_BG_FLAG;
				break;
            case 'l':
				if(optarg != NULL)
                    pktlen = (unsigned int)strtol(optarg, (char **)NULL, 10);
				opt_flag |= SEND_LEN_FLAG;
				break;
            case 'c':
				if(optarg != NULL)
                    pkt_count = (unsigned int)strtol(optarg, (char **)NULL, 10);
				opt_flag |= SEND_COUNT_FLAG;
				break;
            case 'i':
				if(optarg != NULL)
                    interval = (unsigned int)strtol(optarg, (char **)NULL, 10);
				opt_flag |= SEND_INTERVAL_FLAG;
				break;
            case 'f':
				if(optarg != NULL)
                    filename = optarg;
				opt_flag |= SEND_FILE_NAME_FLAG;
				break;
			case 't':
				if(optarg != NULL)
					target = (unsigned int)strtol(optarg, (char **)NULL, 10);
				opt_flag |= SEND_TARGET_UTIL_FLAG;
				break;
			case 'q':
				opt_flag |= SEND_QUIET_FLAG;
				break;
			case 'h':
				opt_flag |= SEND_HELP_FLAG;
                usage();
				break;
            default:
                usage();
                break;
            }
        }
        //printf("radio %d pktlen %d pkt_count %d interval %d filename %s target %d\n",radio,pktlen,pkt_count,interval,filename,target);
        if((opt_flag & SEND_ANY_FLAG)&&(opt_flag & SEND_BG_FLAG))
		{
            printf("-a and -b option cannot be set at the same time\n");
			return;
		}


		if(opt_flag & SEND_BG_FLAG)
		{
		    signal(SIGINT, quit_thread);
			send_bg_traffic(radio,target);
			
			if(!(opt_flag & SEND_QUIET_FLAG))
			{
				pthread_create(&ntid, NULL, read_bg_traffic_result, &radio);		
				while(1)
				{
					sleep(1);
				}
			}
		}
		if((opt_flag & SEND_ANY_FLAG)&&(opt_flag & SEND_FILE_NAME_FLAG))
		{
			read_file_and_send(radio,filename);
		}

        return;
}
