
#ifndef __CHANHANDLE_HPP_
#define __CHANHANDLE_HPP_

typedef struct _dolheader
{
	u32 section_pos[18];
	u32 section_start[18];
	u32 section_size[18];
	u32 bss_start;
	u32 bss_size;
	u32 entry_point;
	u32 padding[7];
} ATTRIBUTE_PACKED dolheader;

u32 LoadChannel(u64 title, bool dol);

#endif /* __CHANHANDLE_HPP_ */
