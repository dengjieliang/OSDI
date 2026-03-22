#include "../Board/common.h"
#include "../Board/dtb.h"
#include "../Lib/utils.h"
#include "../Lib/string.h"

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

typedef struct dtb_struct
{
    unsigned int magic; //固定魔數，用來驗證是否為 DTB（值為 0xd00dfeed，以 big-endian 儲存）
    unsigned int totalsize; //整個 DTB blob 的總長度（bytes），包含 header + 各 block + 空隙
    unsigned int off_dt_struct; //Structure Block 的起始 offset（相對 header 起點）
    unsigned int off_dt_strings;//Strings Block 的起始 offset
    unsigned int off_mem_rsvmap;//Memory Reservation Block 的起始 offset
    unsigned int version;//DTB 格式版本（規格文件對應的版本常見為 17）
    unsigned int last_comp_version; //最低相容版本（例如 version 17 常見為 16）
    unsigned int boot_cpuid_phys; //boot CPU 的 physical ID（對應 CPU node 的 reg）
    unsigned int size_dt_strings; //Strings Block 的長度（bytes）
    unsigned int size_dt_struct; //Structure Block 的長度（bytes）

} DtbT;

bool ReadDTBFile(void* header, FdtHandleNodeFunction callBack, void* user_data)
{
    DtbT* dtb_header = (DtbT*)header;
    unsigned int magic = BigEndianToLittleEndian(&dtb_header->magic);

    if (magic != 0xD00DFEED)
    {
        return false;
    }

    unsigned int totalsize = BigEndianToLittleEndian(&dtb_header->totalsize);
    unsigned int off_dt_struct = BigEndianToLittleEndian(&dtb_header->off_dt_struct);
    unsigned int off_dt_string = BigEndianToLittleEndian(&dtb_header->off_dt_strings);
    unsigned int size_dt_struct = BigEndianToLittleEndian(&dtb_header->size_dt_struct);
    unsigned int size_dt_string = BigEndianToLittleEndian(&dtb_header->size_dt_strings);

    unsigned long struct_base = (unsigned long)header + off_dt_struct;
    unsigned long struct_end = struct_base + size_dt_struct;

    unsigned long string_base = (unsigned long)header + off_dt_string;
    unsigned long string_end = string_base + size_dt_string;

    if (string_end > (unsigned long)header + totalsize || struct_end > (unsigned long)header + totalsize)
    {
        return false;
    }

    int depth = 0;
    char* nodeStack[MAX_DEPTH];
    unsigned long  cursor = struct_base;

    while (cursor + 4 <= struct_end)
    {
        unsigned int token = BigEndianToLittleEndian((void*)cursor);
        cursor += 4;

        if (token == FDT_BEGIN_NODE)
        {
            if (depth >= MAX_DEPTH || depth < 0)
            {
                return false;
            }

            nodeStack[depth] = (char*)(cursor);
            
            bool findNodeName = false;

            while (cursor < struct_end)
            {
                if (*(char*)cursor == '\0')
                {
                    findNodeName = true;
                    break;
                }

                cursor += 1;
            }

            if (findNodeName == false)
            {
                return false;
            }

            //加上\0.
            cursor += 1;

            cursor = ALIGN4(cursor);

            depth += 1;

            callBack(ENTER_NODE, nodeStack, depth, nodeStack[depth - 1], 0, 0, user_data);

            continue;
        }
        else if (token == FDT_END_NODE)
        {
            callBack(LEAVE_NODE, nodeStack, depth, nodeStack[depth - 1], 0, 0, user_data);
            depth -= 1;

            if (depth < 0)
            {
                return false;
            }
            else
            {
                continue;
            }
        }
        else if (token == FDT_PROP)
        {
            if (cursor + 8 > struct_end)
            {
                return false;
            }

            unsigned int valueLength = BigEndianToLittleEndian((void*)cursor);
            cursor += 4;
            unsigned int nameoff = BigEndianToLittleEndian((void*)cursor);
            cursor += 4;

            if (nameoff >= size_dt_string)
            {
                return false;
            }

            char* nodeValueName = (char*)(string_base + nameoff);

            bool findStringEnd = false;

            for (int i = 0; i < size_dt_string - nameoff; i++)
            {
                if (*(nodeValueName + i) == '\0')
                {
                    findStringEnd = true;
                    break;
                }
            }

            if (findStringEnd == false)
            {
                return false;
            }

            unsigned long valuePtr = cursor;

            if (valueLength > struct_end - valuePtr)
            {
                return false;
            }

            cursor += valueLength;
            cursor = ALIGN4(cursor);
            callBack(PROP_NODE, nodeStack, depth, nodeValueName, valuePtr, valueLength, user_data);
            continue;
        }
        else if (token == FDT_NOP)
        {
            continue;
        }
        else if (token == FDT_END)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    return false;
}