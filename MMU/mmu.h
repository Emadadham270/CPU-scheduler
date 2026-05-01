#ifndef MMU_H
#define MMU_H

extern int block_end_time;
void swap(int id, int frameIndex, Frame arr[],int type,int page);
VirtualAddress parse_virtual_address(char *bin_str);
#endif