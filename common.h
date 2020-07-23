#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>
#include <fcntl.h>


//struct list_head;
struct list_head {
	struct list_head *next, *prev;
};

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); \
        	pos = pos->next)

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define	TYPE_EDGE	0
#define	TYPE_FWD	1

struct tuple {
	char addr[16];
	unsigned short port;
	unsigned short id;
	int type;
	unsigned short unused;
} __attribute__((packed));

struct node_info {
	struct list_head list;
	struct list_head macs;
	struct tuple tuple;
	void *other;
};

struct ethernet_header {
    unsigned char dest[6];
    unsigned char source[6];
    unsigned short type;
} __attribute__((packed));


struct ctrl_header {
	unsigned short sid;
	unsigned short did;
	unsigned short num;
	unsigned short type;
	unsigned short unused;
} __attribute__((packed));

#define FRAME_MAX	1500
#define	SEGS_MAX	32

struct framehdr {
	unsigned short sid;
	unsigned short did;
	unsigned short srlen;
	unsigned int len;
	signed short segs[SEGS_MAX];
} __attribute__((packed));

struct frame {
	struct framehdr hdr;
	char data[FRAME_MAX];
} __attribute__((packed));

struct control_frame {
	struct ctrl_header header;
	struct tuple tuple[0];
} __attribute__((packed));

struct config;
struct process_handler {
	struct list_head list;
	unsigned char id;
	struct node_info *peer;
	int (*send)(struct process_handler *this, struct frame *frame);
	int (*receive)(struct process_handler *this, struct frame *frame);
	struct config *conf;
};

struct server {
	char addr[16];
	unsigned short port;
	void *others;
};

struct config {
	struct node_info *self;
	int type;
	union {
		int cfd;
		int tap_fd;
		int con_fd;
	} fd;
	int udp_fd;
	int ctrl_fd;
	struct server server;
	struct list_head peers;
	struct list_head stack;
	struct list_head fwdtable;
	struct list_head *first;
	struct list_head *last;
	int num_handlers;
};

int main_loop(struct config *conf);
int call_stack(struct config *conf, int dir);
int server_msg_register(struct config *conf);
int init_server_connect(struct config *conf, char *addr, unsigned short port);
int server_msg_read(struct config *conf);
int server_request(struct config *conf, const char *cmd, unsigned int len);
int server_reply(struct config *conf, char *buffer, unsigned int *plen);
int register_handler(struct process_handler *handler, struct config *conf);
int unregister_handler(struct process_handler *handler, struct config *conf);
int init_config(struct config *conf, int type);
int init_self(struct config *conf, char *addr, unsigned short port);

void INIT_LIST_HEAD(struct list_head *list);
void list_add_tail(struct list_head *new, struct list_head *head);
void list_add(struct list_head *new, struct list_head *head);
void list_del(struct list_head *entry);
