#include "common.h"

struct segs_entry {
	struct list_head list;
	unsigned short srlen;
	char dst_mac[6];
	unsigned short segs[SEGS_MAX];
};

int read_from_tap(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int fd = -1;
	size_t len;

	if (obj->conf->type == TYPE_EDGE) {
		fd = obj->conf->fd.cfd;
		len = read(fd, frame->data, sizeof(frame->data));
		frame->hdr.len = len + sizeof(struct framehdr);
	}

	return ret;
}

int send_frame(struct process_handler *obj, struct frame *frame);
int end_receive(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int fd = -1;
	size_t len;

	if (obj->conf->type == TYPE_EDGE) {
		fd = obj->conf->fd.cfd;
		len = write(fd, frame->data, sizeof(frame->data));
	} else if (obj->conf->type == TYPE_FWD) {
		ret = send_frame(obj, frame);
	}

	return ret;
}

static struct process_handler end_handler = {
	.send = read_from_tap,
	.receive = end_receive, 
};

int segs_setting(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int i;
	struct ethernet_header eh;
	struct framehdr *hdr = &frame->hdr;
	struct list_head *tmp;
	
	memcpy(&eh, frame->data, sizeof(eh));
	for (i = 0; i< SEGS_MAX; i++) {
		hdr->segs[i] = -1;
	}

	list_for_each(tmp, &obj->conf->fwdtable) {
		struct segs_entry *tmp_entry = list_entry(tmp, struct segs_entry, list);
		if (!memcmp(eh.dest, tmp_entry->dst_mac, 6)) {
			unsigned short srlen = tmp_entry->srlen;
			hdr->srlen = srlen;	
			for (i = 0; i< srlen; i++) {
				hdr->segs[i] = tmp_entry->segs[i];
			}
			break;
		}
	}
	return ret;
}

int segs_forwarding(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int i;
	signed short curr = -1;
	struct framehdr *hdr = &frame->hdr;
	struct list_head *tmp;
	struct node_info *peer;
	
	for (i = 0; i < SEGS_MAX && hdr->segs[i] != -1; i++) {
		if (hdr->segs[i] == 0) {
			continue;
		}
		curr = hdr->segs[i];
		hdr->segs[i] = 0;
		break;
	}
	if (curr == -1) {
		ret = -2;
	}
	list_for_each(tmp, &obj->conf->peers) {
		peer = list_entry(tmp, struct node_info, list);
		if (peer->tuple.id == curr) {
			printf("ID:%d online\n", peer->tuple.id);
			obj->peer = peer;
			break;
		}
	}
	if (!obj->peer) {
		ret = -1;
	}

	return ret;
}

static struct process_handler routing_handler = {
	.send = segs_setting,
	.receive = segs_forwarding,
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


int init_tap(struct config *conf)
{
	int fd = -1;
	struct ifreq ifr;

	conf->fd.cfd = 1025;
	if (conf->type != TYPE_EDGE) {
		return 0;
	}	

	if( (fd = open("/dev/net/tun", O_RDWR)) < 0) {
		exit(-1);
	}
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags |= IFF_NO_PI;
	ifr.ifr_flags |= IFF_TAP;
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", "tap0");
	ioctl(fd, TUNSETIFF, (void *)&ifr);

	conf->fd.cfd = fd;

	return 0;
}

void get_parse_segs(struct config *conf)
{
	char *cmd = "GET_SR";
	char segs_buff[FRAME_MAX];
	unsigned int len = sizeof(segs_buff);

	server_request(conf, cmd, strlen(cmd));
	server_reply(conf, segs_buff, &len);
	
	// TODO
}

int main(int argc, char **argv)
{
	char serverIP[16];
	char localIP[16];
	unsigned short serverPORT;
	unsigned short localPORT;
	struct config conf;
	int type;

	if (argc != 6) {
		printf("./a.out serverIP serverPORT localIP localPORT [EDGE|FWD]\n");
	}
	strcpy(serverIP, argv[1]);
	serverPORT = atoi(argv[2]);
	strcpy(localIP, argv[3]);
	localPORT = atoi(argv[4]);
	type = atoi(argv[5]);

	init_config(&conf, type);	
	init_tap(&conf);
	init_self(&conf, localIP, localPORT);
	
	register_handler(&end_handler, &conf);
	register_handler(&routing_handler, &conf);
	register_handler(&udp_handler, &conf);
	
	init_server_connect(&conf, serverIP, serverPORT);

	server_msg_register(&conf);
	server_msg_read(&conf);

	get_parse_segs(&conf);

	main_loop(&conf);
	
	return 0;
}
