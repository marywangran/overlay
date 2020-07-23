#include "common.h"

int read_from_console(struct process_handler *obj, struct frame *frame)
{
	size_t len = 0;
	int fd = obj->conf->fd.cfd;
	struct list_head *tmp;
	struct node_info *peer;
	char buf[1024];

	
	len = read(fd, buf, 1024);
	if (len > 0 && !strncmp(buf, "list", strlen("list"))) {
	
		list_for_each(tmp, &obj->conf->peers) {
			peer = list_entry(tmp, struct node_info, list);
			printf("ID:%d online\n", peer->tuple.id);
		}
		return 1;
	} else {
		char IDchar[4] = {0};
		int ID = 0;
		strncpy(IDchar, buf, 1);
		ID = atoi(IDchar);
		list_for_each(tmp, &obj->conf->peers) {
			peer = list_entry(tmp, struct node_info, list);
			if (peer->tuple.id == ID) {
				obj->peer = peer;
			}
		}
		if (!obj->peer) {
			return -1;
		}
		len = strlen(buf) - 2;
		strncpy(frame->data, buf + 2, len);
		frame->hdr.len = len + sizeof(struct framehdr);
	}


	return 0;
}

int write_to_console(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int fd = obj->conf->fd.cfd;
	size_t len;

	printf("From %d:%s\n", frame->hdr.sid, frame->data);

	return ret;
}

static struct process_handler console_handler = {
	.send = read_from_console,
	.receive = write_to_console,
};

int _routing(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	// Nothing todo
	return ret;
}


int _learning(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	// Nothing todo
	return ret;
}
static struct process_handler routing_handler = {
	.send = _routing,
	.receive = _learning,
};

int encode_frame(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;

	frame->hdr.sid = obj->conf->self->tuple.id;
	frame->hdr.did = 0;
	frame->hdr.srlen = 0;
	if (obj->peer) {
		frame->hdr.did = obj->peer->tuple.id;
	} 

	return ret;
}

int decode_frame(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;

	if (frame->hdr.did != 0 && frame->hdr.did != obj->conf->self->tuple.id) {
		ret = -1;
	}

	return ret;
}
static struct process_handler protocol_handler = {
	.send = encode_frame,
	.receive = decode_frame,
};

int encrypt(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int i = sizeof(struct framehdr);
	char *c = (char *)frame;
	int len = frame->hdr.len - i;

	for (; i < len; i++) {
		c[i] = c[i] + 1;
	}
	return ret;
}

int decrypt(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int i = sizeof(struct framehdr);
	char *c = (char *)frame;
	int len = frame->hdr.len - i;

	for (; i < len; i++) {
		c[i] = c[i] - 1;
	}
	return ret;
}

static struct process_handler enc_handler = {
	.send = encrypt,
	.receive = decrypt,
};

int send_frame(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int fd = obj->conf->udp_fd;
	size_t len = 0;
	struct node_info *peer = obj->peer;
	struct config *conf = obj->conf;
	struct sockaddr_in addr;
	int addr_len = sizeof(struct sockaddr_in); 

	bzero(&addr,sizeof(addr));
	addr.sin_family = AF_INET;
	if (peer) {
		addr.sin_port = htons(peer->tuple.port);
		addr.sin_addr.s_addr = inet_addr(peer->tuple.addr);
		len = sendto(fd, frame, frame->hdr.len, 0, (struct sockaddr *)&addr, addr_len);
	} else {
		struct list_head *tmp;
		list_for_each(tmp, &conf->peers) {
			peer = list_entry(tmp, struct node_info, list);
			addr.sin_port = htons(peer->tuple.port);
			addr.sin_addr.s_addr = inet_addr(peer->tuple.addr);
			len = sendto(fd, frame, frame->hdr.len, 0, (struct sockaddr *)&addr, addr_len);
		}
		
	}
	return ret;
}

int receive_frame(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	size_t len = 0;
	struct list_head *tmp;
	struct node_info *peer;
	struct sockaddr_in addr;
	char *saddr;
	int port;
	int addr_len = sizeof(struct sockaddr_in); 

	bzero (&addr, sizeof(addr));
	
	len = recvfrom(obj->conf->udp_fd, frame, sizeof(struct frame), 0 , (struct sockaddr *)&addr ,&addr_len);
	frame->hdr.len = len;

	list_for_each(tmp, &obj->conf->peers) {
		peer = list_entry(tmp, struct node_info, list);
		saddr = inet_ntoa(addr.sin_addr);
		port = ntohs(addr.sin_port);
		if (!memcmp(saddr, peer->tuple.addr, strlen(saddr)) && port == peer->tuple.port) {
			obj->peer = peer;
		}	
	}
	if (!obj->peer) {
		ret = -1;
	}
	
	return ret;
}
static struct process_handler udp_handler = {
	.send = send_frame,
	.receive = receive_frame,
};

int init_console(struct config *conf)
{
	conf->fd.cfd = 0;
}

int main(int argc, char **argv)
{
	char serverIP[16];
	char localIP[16];
	unsigned short serverPORT;
	unsigned short localPORT;
	struct config conf;

	if (argc != 5) {
		printf("./a.out serverIP serverPORT localIP localPORT\n");
	}
	strcpy(serverIP, argv[1]);
	serverPORT = atoi(argv[2]);
	strcpy(localIP, argv[3]);
	localPORT = atoi(argv[4]);

	init_config(&conf, TYPE_EDGE);	
	init_console(&conf);
	init_self(&conf, localIP, localPORT);
	
	register_handler(&console_handler, &conf);
	register_handler(&routing_handler, &conf);
	register_handler(&protocol_handler, &conf);
	register_handler(&enc_handler, &conf);
	register_handler(&udp_handler, &conf);
	
	init_server_connect(&conf, serverIP, serverPORT);

	server_msg_register(&conf);
	server_msg_read(&conf);

	main_loop(&conf);
	
	return 0;
}
