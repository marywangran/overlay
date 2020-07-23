#include "common.h"


struct mac_entry {
	struct list_head list;
	struct list_head node; //struct hlist_node;
	char mac[6];
	struct node_info *peer;
};

int read_from_tap(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int fd = obj->conf->fd.cfd;
	size_t len;

	len = read(fd, frame->data, sizeof(frame->data));
	frame->hdr.len = len + sizeof(struct framehdr);

	return ret;
}

int write_to_tap(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	int fd = obj->conf->fd.cfd;
	size_t len = frame->hdr.len - sizeof(struct framehdr);

	len = write(fd, frame->data, len);

	return ret;
}

static struct process_handler tap_handler = {
	.send = read_from_tap,
	.receive = write_to_tap,
};

int frame_routing(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	struct ethernet_header eh;
	struct list_head *tmp;
	
	memcpy(&eh, frame->data, sizeof(eh));
	list_for_each(tmp, &obj->conf->fwdtable) {
		struct mac_entry *tmp_entry = list_entry(tmp, struct mac_entry, node);
		if (!memcmp(eh.dest, tmp_entry->mac, 6)) {
			obj->peer = tmp_entry->peer;
			break;
		}
	}
	return ret;
}


int mac_learning(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;
	struct mac_entry *entry = NULL;
	struct ethernet_header eh;
	struct list_head *tmp;
	
	memcpy(&eh, frame->data, sizeof(eh));
	list_for_each(tmp, &obj->conf->fwdtable) {
		struct mac_entry *tmp_entry = list_entry(tmp, struct mac_entry, node);
		if (!memcmp(eh.source, tmp_entry->mac, 6)) {
			entry = tmp_entry;
			break;
		}
	}
	
	if (entry) {
		list_del(&entry->list);
		list_del(&entry->node);
	} else {
		entry = (struct mac_entry *)calloc(1, sizeof(struct mac_entry));
	}

	if (!entry) {
		printf("Alloc entry failed\n");
		return -1;
	}
	
	memcpy(entry->mac, eh.source, 6);
	entry->peer = obj->peer;
	INIT_LIST_HEAD(&entry->list);
	list_add(&entry->list, &entry->peer->macs);
	INIT_LIST_HEAD(&entry->node);
	list_add(&entry->node, &obj->conf->fwdtable);

	return ret;
}
static struct process_handler routing_handler = {
	.send = frame_routing,
	.receive = mac_learning,
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
	printf("####1000 encode_frame :%d  %d\n", frame->hdr.sid, frame->hdr.did);

	return ret;
}

int decode_frame(struct process_handler *obj, struct frame *frame)
{
	int ret = 0;

	printf("#### decode_frame: sid:%d  did:%d\n", frame->hdr.sid, frame->hdr.did);
	if (frame->hdr.did != 0 && frame->hdr.did != obj->conf->self->tuple.id) {
		ret = -1;
	}
	if (frame->hdr.segs != 0) {
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


int init_tap(struct config *conf)
{
	int fd = -1;
	struct ifreq ifr;
	

	if( (fd = open("/dev/net/tun", O_RDWR)) < 0) {
		exit(-1);
	}
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags |= IFF_NO_PI;
	ifr.ifr_flags |= IFF_TAP;
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", "tap0");
	ioctl(fd, TUNSETIFF, (void *)&ifr);

	conf->fd.cfd = fd;
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
	init_tap(&conf);
	init_self(&conf, localIP, localPORT);
	
	register_handler(&tap_handler, &conf);
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
