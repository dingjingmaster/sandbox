#ifndef EFS_COMMON_TYPES_H
#define EFS_COMMON_TYPES_H
#include <stdint.h>

/**
 * @brief 这里定义公共的类型，这些类型对驱动和上层应用都很重要
 */

#ifndef C_IN
#define C_IN
#endif

#ifndef C_OUT
#define C_OUT
#endif


// 检测 编译器 是否支持 c11 标准
#ifndef C_SUPPORTED_C11
#if defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 7
#define C_SUPPORTED_C11 1
#elif defined(__clang__) && __clang_major__ >= 3 && __clang_minor__ >= 0
#define C_SUPPORTED_C11 1
#else
#define C_SUPPORTED_C11 0
#endif
#endif

// 计算结构体中成员在结构体中的偏移位置
#ifndef C_STRUCT_OFFSET_OF
#define C_STRUCT_OFFSET_OF(structType, member)                                  offsetof(structType, member)
#endif

// 检查结构体大小是否符合预期
#ifndef C_STRUCT_SIZE_CHECK
#if C_SUPPORTED_C11
#define C_STRUCT_SIZE_CHECK(structType, expectedSize)                           static_assert(sizeof(structType) == expectedSize, "struct '"#structType"' size is wrong");
#else
#define C_STRUCT_SIZE_CHECK(structType, expectedSize)                           typedef char _macros_check_size##structType[(sizeof(structType) == expectedSize ? 1 : -1)];
#endif
#endif

// 检查类型大小是否符合预期
#ifndef C_TYPE_SIZE_CHECK
#define C_TYPE_SIZE_CHECK(typeT, sizeT)                                         C_STRUCT_SIZE_CHECK(typeT, sizeT)
#endif

// 定义 int 类型
#ifndef cint8
typedef signed char                                                             cint8;
C_TYPE_SIZE_CHECK(cint8, 1)
#endif

#ifndef cuint8
typedef unsigned char                                                           cuint8;
C_TYPE_SIZE_CHECK(cuint8, 1)
#endif

#ifndef cint16
typedef signed short                                                            cint16;
C_TYPE_SIZE_CHECK(cint16, 2)
#endif

#ifndef cuint16
typedef unsigned short                                                          cuint16;
C_TYPE_SIZE_CHECK(cuint16, 2)
#endif

#ifndef cint32
typedef signed int                                                              cint32;
C_TYPE_SIZE_CHECK(cint32, 4)
#endif

#ifndef cuint32
typedef unsigned int                                                            cuint32;
C_TYPE_SIZE_CHECK(cuint32, 4)
#endif

#ifndef cint64
typedef signed long                                                             cint64;
C_TYPE_SIZE_CHECK(cint64, 8)
#endif

#ifndef cuint64
typedef unsigned long                                                           cuint64;
C_TYPE_SIZE_CHECK(cuint64, 8)
#endif

#ifndef cfloat
typedef float                                                                   cfloat;
C_TYPE_SIZE_CHECK(cfloat, 4)
#endif

#ifndef cdouble
typedef double                                                                  cdouble;
C_TYPE_SIZE_CHECK(cdouble, 8)
#endif

#ifndef cpointer
typedef void*                                                                   cpointer;
C_TYPE_SIZE_CHECK(cpointer, 8)
#endif

#ifndef C_INT8
#define C_INT8(x)                                                               ((cint8) x)
#endif

#ifndef C_UINT8
#define C_UINT8(x)                                                              ((cuint8) x)
#endif

#ifndef C_INT16
#define C_INT16(x)                                                              ((cint16) x)
#endif

#ifndef C_UINT16
#define C_UINT16(x)                                                             ((cuint16) x)
#endif

#ifndef C_INT32
#define C_INT32(x)                                                              ((cint32) x)
#endif

#ifndef C_UINT32
#define C_UINT32(x)                                                             ((cuint32) x)
#endif

#ifndef C_INT64
#define C_INT64(x)                                                              ((cint64) x)
#endif

#ifndef C_UINT64
#define C_UINT64(x)                                                             ((cuint64) x)
#endif

#ifndef C_POINTER
#define C_POINTER(x)                                                            ((C_POINTER) x)
#endif

#ifndef C_INT64_CONSTANT
#define C_INT64_CONSTANT(val)                                                   (val##L)
#endif

#ifndef C_UINT64_CONSTANT
#define C_UINT64_CONSTANT(val)                                                  (val##L)
#endif

#ifndef C_INT8_MAX
#define C_INT8_MAX                                                              ((cint8)    0x7F)
#endif

#ifndef C_INT8_MIN
#define C_INT8_MIN                                                              ((cint8)    (-C_INT8_MAX) - 1)
#endif

#ifndef C_UINT8_MAX
#define C_UINT8_MAX                                                             ((cuint8)   0xFF)
#endif

#ifndef C_UINT8_MIN
#define C_UINT8_MIN                                                             ((cuint8)   0x00)
#endif

#ifndef C_INT16_MAX
#define C_INT16_MAX                                                             ((cint16)   0x7FFF)
#endif

#ifndef C_INT16_MIN
#define C_INT16_MIN                                                             ((cint16)   (-C_INT16_MAX) - 1)
#endif

#ifndef C_UINT16_MAX
#define C_UINT16_MAX                                                            ((cuint16)  0xFFFF)
#endif

#ifndef C_UINT16_MIN
#define C_UINT16_MIN                                                            ((cuint16)  0x0000)
#endif

#ifndef C_INT32_MAX
#define C_INT32_MAX                                                             ((cint32)   0x7FFFFFFF)
#endif

#ifndef C_INT32_MIN
#define C_INT32_MIN                                                             ((cint32)   (-C_INT32_MAX) - 1)
#endif

#ifndef C_UINT32_MAX
#define C_UINT32_MAX                                                            ((cuint32)  0xFFFFFFFF)
#endif

#ifndef C_UINT32_MIN
#define C_UINT32_MIN                                                            ((cuint32)  0x00000000)
#endif

#ifndef C_INT64_MAX
#define C_INT64_MAX                                                             ((cint64)   0x7FFFFFFFFFFFFFFF)
#endif

#ifndef C_INT64_MIN
#define C_INT64_MIN                                                             ((cint64)   (-C_INT64_MAX) - 1)
#endif

#ifndef C_UINT64_MAX
#define C_UINT64_MAX                                                            ((cuint64)  0xFFFFFFFFFFFFFFFF)
#endif

#ifndef C_UINT64_MIN
#define C_UINT64_MIN                                                            ((cuint64)  0x0000000000000000)
#endif

// 自然常数 e
#ifndef C_E
#define C_E                                                                     2.7182818284590452353602874713526624977572470937000
#endif

// log 以 e 为底 2 的对数
#ifndef C_LN_2
#define C_LN_2                                                                  0.69314718055994530941723212145817656807550013436026
#endif

// log 以 e 为底 10 的对数
#ifndef C_LN_10
#define C_LN_10                                                                 2.3025850929940456840179914546843642076011014886288
#endif

// pi
#ifndef C_PI
#define C_PI                                                                    3.1415926535897932384626433832795028841971693993751
#endif

// pi / 2
#ifndef C_PI_DIV_2
#define C_PI_DIV_2                                                              1.5707963267948966192313216916397514420985846996876
#endif

// pi / 4
#ifndef C_PI_DIV_4
#define C_PI_DIV_4                                                              0.78539816339744830961566084581987572104929234984378
#endif

// 根号 2 的结果
#ifndef C_SQRT_2
#define C_SQRT_2                                                                1.4142135623730950488016887242096980785696718753769
#endif

// 定义 bool 类型
#ifdef __cplusplus
#else
#ifndef bool
typedef int                                                                     bool;
#endif
#ifndef false
#define false                                                                   (0)
#endif
#ifndef true
#define true                                                                    (!false)
#endif
#endif

/**
 * @brief 函数可见性，gcc可见性分为以下几种情况(__attribute__((visibility(""))))：
 *  1. default：默认可见（函数在程序的任何地方可见）
 *  2. hidden：隐藏可见性。函数在链接时候不可见，对于外部链接的符号，将无法从其它目标文件中引用
 *  3. protected：受保护可见。函数在链接时候可见，但只能被其所在的目标文件或具有相同共享库的目标文件引用
 */
#undef C_SYMBOL_EXPORT
#undef C_SYMBOL_IMPORT
#undef C_SYMBOL_HIDDEN
#undef C_SYMBOL_PROTECTED
#if (defined(_WIN32) || defined(__CYGWIN__))
#define C_SYMBOL_EXPORT __declspec(dllexport)
#define C_SYMBOL_IMPORT __declspec(dllimport)
#define C_SYMBOL_HIDDEN
#define C_SYMBOL_PROTECTED
#elif __GNUC__ >= 4
#define C_SYMBOL_IMPORT
#define C_SYMBOL_HIDDEN     __attribute__((visibility("hidden")))
#define C_SYMBOL_EXPORT     __attribute__((visibility("default")))
#define C_SYMBOL_PROTECTED  __attribute__((visibility("protected")))
#else
#define C_SYMBOL_EXPORT
#define C_SYMBOL_IMPORT
#define C_SYMBOL_HIDDEN
#define C_SYMBOL_PROTECTED
#endif

// __func__
#ifndef C_STRFUNC
#if defined (__GNUC__) && defined (__cplusplus)
#define C_STRFUNC                                                               ((const char*) (__PRETTY_FUNCTION__))
#elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define C_STRFUNC                                                               ((const char*) (__func__))
#elif defined (__GNUC__) || (defined(_MSC_VER) && (_MSC_VER > 1300))
#define C_STRFUNC                                                               ((const char*) (__FUNCTION__))
#else
#define C_STRFUNC                                                               ((const char*) ("???"))
#endif
#endif

// __LINE__
#ifndef C_LINE
#define C_LINE                                                                  (__LINE__)
#endif

// extern "c"
#undef C_BEGIN_EXTERN_C
#undef C_END_EXTERN_C
#ifdef  __cplusplus
#define C_BEGIN_EXTERN_C                                                        extern "C" {
#define C_END_EXTERN_C                                                          }
#else
#define C_BEGIN_EXTERN_C
#define C_END_EXTERN_C
#endif

#undef C_MAX
#define C_MAX(a, b)                                                             (((a) > (b)) ? (a) : (b))

#undef C_MIN
#define C_MIN(a, b)                                                             (((a) < (b)) ? (a) : (b))

#undef C_ABS
#define C_ABS(a)                                                                (((a) < 0) ? -(a) : (a))

#undef C_N_ELEMENTS
#define C_N_ELEMENTS(arr)                                                       (sizeof(arr) / sizeof((arr)[0]))

#define C_ASCII_IS_UPPER(c)                                                     ((c) >= 'A' && (c) <= 'Z')
#define C_ASCII_IS_LOWER(c)                                                     ((c) >= 'a' && (c) <= 'z')
#define C_ASCII_IS_ALPHA(c)                                                     (C_ASCII_IS_UPPER (c) || C_ASCII_IS_LOWER(c))
#define C_ASCII_TO_UPPER(c)                                                     (C_ASCII_IS_LOWER (c) ? (c) - 'a' + 'A' : (c))
#define C_ASCII_TO_LOWER(c)                                                     (C_ASCII_IS_UPPER (c) ? (c) - 'A' + 'a' : (c))
#define C_ASCII_IS_SPACE(c)                                                     ((c) == ' ' || (c) == '\f' || (c) == '\n' || (c) == '\r' || (c) == '\t' || (c) == '\v')


#define MAX_PATH_BYTES              4096            // 最大路径长度
#define MAX_FILENAME                1024            // 最大文件名长度
#define MAX_KEY_LEN                 32              // 最大加解密key长度
#define MAX_PASSWD_LEN              MAX_KEY_LEN     // 最大密码长度(需要和加解密key长度严格一致)

#define ENCRYPT_BLOCK_SIZE          (0x200)         // 加密块大小 512
#define ENCRYPT_BLOCK_ALIGN         (ENCRYPT_BLOCK_SIZE-1)

#ifndef ENCRYPT_KEY_LEN
#define ENCRYPT_KEY_LEN             64              // 加密key长度
#endif

#ifndef MD5_VALUE_LEN
#define MD5_VALUE_LEN               16              // MD5 长度
#endif

#ifndef SM3_VALUE_LEN
#define SM3_VALUE_LEN               32              // SM3 长度
#endif

#define USER_MAX_PASSWD_LEN         32              // 用户密码最大长度
#define USER_KEY_LEN                32              // 用户key长度

#define USER_ID_LEN                 32
#define HEAD_DATA_SIZE              32


// 文件类型
typedef enum
{
    FILE_TYPE_NONE              = 0x0000,           // 没加密的普通文件
    FILE_TYPE_SANDBOX           = 0x0001,           // 沙盒文件
} FileType;

// 加密模式
typedef enum
{
    ENCRYPT_MODE_ECB            = 0,
    ENCRYPT_MODE_CBC            = 1,
    ENCRYPT_MODE_CFB            = 2,
} EncryptMode;

// 透明加解密工作模式
typedef enum
{
    WORK_MODE_NORMAL            = 0x0,              // 正常工作模式，启动关联后自动进入该模式。正常加解密(默认)
    WORK_MODE_USERSELF          = 0x1,              // 用户个人工作模式，关联进程只能读写未加密的文件，但不能打开加密文件
    WORK_MODE_WRITEENCRYPT      = 0x2,              // 关联进程不能打开加密的策略文件，能打开明文文件、但无法编辑。
                                                    // 客户端没有登录或异常退出后自动进入该模式
} WorkMode;

// 审计日志类型，要上传到服务器
typedef enum
{
    LOG_STATE_STOP              = 0x00000000,       // 不记录任何日志
    LOG_STATE_CREATE            = 0x00000001,       // 记录新建文件日志     = FILE_CREATE
    LOG_STATE_READ              = 0x00000002,       // 记录读取文件日志     = FILE_READ
    LOG_STATE_WRITE             = 0x00000004,       // 记录修改文件日志     = FILE_MODIFY
    LOG_STATE_RENAME            = 0x00000008,       // 记录修改文件名字日志 = FILE_RENAME
    LOG_STATE_DELETE            = 0x00000010,       // 记录删除文件日志     = FILE_DELETE
    LOG_STATE_ENCRYPT           = 0x00000020,       // 记录落地加密文件日志 = FILE_ENCRYPT
    LOG_STATE_DECRYPT           = 0x00000040,       // 记录落地解密文件日志 = FILE_DECRYPT
    LOG_STATE_DENIED            = 0x00000080,       // 记录禁止操作日志     = DENIED
    LOG_STATE_ALLOW             = 0x00000100,       // 记录允许操作日志     = ALLOW
    LOG_STATE_WARNING           = 0x00000200,       // 记录警告操作日志     = WARN
    LOG_STATE_SUPER_ENCRYPT     = 0x00000400,       // 记录特权加密日志     = SUPER_ENC
    LOG_STATE_SUPER_DECRYPT     = 0x00000800,       // 记录特权解密日志     = SUPER_DEC
    LOG_STATE_OPEN              = 0x00001000,       // 记录文件打开日志     = FILE_OPEN
    LOG_STATE_CLOSE             = 0x00002000,       // 记录文件关闭日志     = FILE_CLOSE
    LOG_STATE_SAVEAS            = 0x00004000,       // 记录文件另存为日志   = FILE_SAVEAS
    LOG_STATE_COPY              = 0x00008000,       // 记录文件复制日志     = FILE_COPY
    LOG_STATE_PRINT             = 0x00010000,       // 记录文件打印日志     = FILE_PRINT
    LOG_STATE_NETSTATUS         = 0x00020000,       // 记录网络状态改变日志 = NETSTATUS_CHANGED
    LOG_STATE_PROCESS_START     = 0x00040000,       // 记录监控进程状态日志 = PROCESS_START
    LOG_STATE_PROCESS_STOP      = 0x00080000,       // 记录监控进程状态日志 = PROCESS_STOP
    LOG_STATE_LAST              = 0xFFFFFFFF,
} LogState;

// 审计日志格式化相关宏...

#pragma pack(push, 1)

// 所有加解密文件通用头信息
typedef struct _EfsFileHeader
{
    uint32_t            magic;                      // magic
    uint32_t            magicID;                    //
    uint16_t            version;                    //
    uint16_t            headSize;                   // 文件头大小, EFS_FILE_HEADER_SIZE
    uint16_t            fileType;                   // FileType
    uint16_t            headArith;                  // 头部数据加密算法
    uint8_t             prompt[68];                 // FILE_PROMPT
    uint16_t            reserved[2];                // Align
// off = 64+24 = 88
    uint8_t             authorID[USER_ID_LEN];      // 作者ID/文件类型
    uint8_t             departID[USER_ID_LEN];      // 作者所在部门ID/文件类型扩展/设备ID 沙盒
    uint32_t            userState;                  // 当前用户读写删除权限/签名状态等
    uint16_t            headItems;                  // items in file head
    uint16_t            fileItems;                  // used to save the item numbers save in the file when HeadType = 0
// off = 160 = 0xA0

// begin to encrypt
    uint8_t             dataKey[MAX_KEY_LEN];       // 数据密钥长度
    uint32_t            dataKeyAdler;               //
    uint32_t            masterKeyAdler;             //
    uint16_t            keyArith;                   // 加密 dataKey 的算法
    uint16_t            dataArith;                  // 数据加密算法
    uint16_t            dataMode;                   // 数据加密模式
    uint16_t            headDataSize;               // dataAdler size
// off = 208 = 0xD0(dataKey - dataAdler = 0x30)
    uint32_t            dataAdler;                  // the first (file size(<1024) or 1024(filesize > 1024))
    uint32_t            headAdler;                  // 
    uint32_t            reserved1[2];
// 160+32+4*8 = 224
    uint8_t             headData[HEAD_DATA_SIZE];   //
} EfsFileHeader;
C_STRUCT_SIZE_CHECK(EfsFileHeader, 256)



#pragma pack(pop)


#endif
