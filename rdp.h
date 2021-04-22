struct rdp {  
  unsigned char       flag;
  unsigned char       pktseq;
  unsigned char       ackseq;
  unsigned char       unassigned;
  int                 senderid;
  int                 recvid;
  int                 metadata;
  unsigned char       payload[1000];
};

int printrdp(struct rdp *pakke) {
  printf("{ flag==%x, pktseq==%x, ackseq==%x, senderid==%x, recvid==%x, metadata==%x, payload==\"%s\" }\n",
         pakke->flag,
         pakke->pktseq,
         pakke->ackseq,
         pakke->senderid,
         pakke->recvid,
         pakke->metadata,
         pakke->payload);
  return 1;
}
