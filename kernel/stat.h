#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct stat {
  int dev;     // File system's disk device   文件所在设备的设备号
  uint ino;    // Inode number                文件索引节点
  short type;  // Type of file                文件类型
  short nlink; // Number of links to file     连接数
  uint64 size; // Size of file in bytes       文件大小
};
