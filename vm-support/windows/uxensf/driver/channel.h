#define RING_SIZE 262144

NTSTATUS ChannelConnect(void);
NTSTATUS ChannelPrepareReq(void);
NTSTATUS ChannelSend(char* buffer, int count);
NTSTATUS ChannelRecv(char* buffer, int buffer_size, int *recv_size);
void ChannelDisconnect(void);

