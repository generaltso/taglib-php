/**
 * TagLib implementation and php extension
 * Mostly for manipulating Tags
 * But also reading audioProperties
 * Supports MPEG (MP3) and OGG, soon(tm): FLAC */

/**
 * standard libs */
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <sstream>

/**
 * .h required for php extensions */
#include "php_taglib.h"

/**
 * taglib reports errors through std::cerr
 * let's expose these messages to PHP 
 * so they can actually be detected and handled 
 *
 * This idea came from a post I found on Google Groups
 * posted August 15, 2002 by Chris Uzdavinis
 * https://groups.google.com/d/msg/borland.public.cppbuilder.language/Uua6t3VhELA/vGyq-ituxN8J
 */
static std::stringstream taglib_cerr;
class Stream_Swapper {
public:
    Stream_Swapper(std::ostream &orig, std::ostream &replacement) : buf_(orig.rdbuf()), str_(orig)
    {
        orig.rdbuf(replacement.rdbuf());
    }
    ~Stream_Swapper()
    {
        str_.rdbuf(buf_);
    }
private:
    std::streambuf *buf_;
    std::ostream &str_;
} swapper(std::cerr, taglib_cerr);

/**
 * exceptions yay */
#include "zend_exceptions.h"
#include "ext/spl/spl_exceptions.h"

static bool taglib_error()
{
    bool retval = false;
    /**
     * all TagLib errors happen to be prefixed with either
     * "TagLib: " - debug() - tdebug.cpp
     * "*** " - debugData() - tdebug.cpp
     * and std::cerr sometimes contains more than just '\0' */
    if(taglib_cerr.peek() == 'T' || taglib_cerr.peek() == '*')
    {
        char errorMessage[255];
        taglib_cerr.getline(errorMessage,255);
        php_error(E_WARNING, "%s", errorMessage);
        retval = true;
    }

//    zend_replace_error_handling( EH_NORMAL, NULL, NULL TSRMLS_CC);

    return retval;
}

/**
 * baby duck syndrome - using strings in switch statements
 * 
 * Thanks to Captain Oblivious:
 * http://stackoverflow.com/a/16721551
 *
 * NOTE: This requires C++11. 
 * NOTE: I've manually editted the Makefile to add the -std=c++11 flag
 */
constexpr unsigned int _charArrForSwitch(const char* str, int index = 0)
{
    return !str[index] ? 0x1505 : (_charArrForSwitch(str, index + 1) * 0x21) ^ str[index];
}

constexpr unsigned int operator"" _CASE ( const char str[], size_t size )
{
    return _charArrForSwitch(str);
}

/**
 * Image tags can be "get" and "set" with base64-encoded data 
 * 
 * When setting, you don't need a file handle, which might be desirable
 * with generated images, such as those passed as data-uris from 
 * JavaScript or PHP's gd functions.
 * 
 * When getting you don't have to specify a file to write just to
 * get the embedded image data. 
 * 
 * XXX #include "ext/standard/base64.h"
 * XXX I couldn't figure out what/how to link with to get php functions,
 * XXX so there's this implementation of base64 en/decoding taken from: 
 */

/**
 * github.com/littlstar/b64.c
 * MIT license */
static const char b64_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};

static char *b64_encode (const unsigned char *src, size_t len) {
    int i = 0;
    int j = 0;
    char *enc = NULL;
    size_t size = 0;
    unsigned char buf[4];
    unsigned char tmp[3];
    // alloc
    enc = (char *) malloc(0);
    if (NULL == enc) { return NULL; }
    // parse until end of source
    while (len--) {
        // read up to 3 bytes at a time into `tmp'
        tmp[i++] = *(src++);
        // if 3 bytes read then encode into `buf'
        if (3 == i) {
            buf[0] = (tmp[0] & 0xfc) >> 2;
            buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
            buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
            buf[3] = tmp[2] & 0x3f;
            // allocate 4 new byts for `enc` and
            // then translate each encoded buffer
            // part by index from the base 64 index table
            // into `enc' unsigned char array
            enc = (char *) realloc(enc, size + 4);
            for (i = 0; i < 4; ++i) {
                enc[size++] = b64_table[buf[i]];
            }
            // reset index
            i = 0;
        }
    }
    // remainder
    if (i > 0) {
        // fill `tmp' with `\0' at most 3 times
        for (j = i; j < 3; ++j) {
            tmp[j] = '\0';
        }
        // perform same codec as above
        buf[0] = (tmp[0] & 0xfc) >> 2;
        buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
        buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
        buf[3] = tmp[2] & 0x3f;
        // perform same write to `enc` with new allocation
        for (j = 0; (j < i + 1); ++j) {
            enc = (char *) realloc(enc, size);
            enc[size++] = b64_table[buf[j]];
        }
        // while there is still a remainder
        // append `=' to `enc'
        while ((i++ < 3)) {
            enc = (char *) realloc(enc, size);
            enc[size++] = '=';
        }
    }
    // Make sure we have enough space to add '\0' character at end.
    enc = (char *) realloc(enc, size + 1);
    enc[size] = '\0';
    return enc;
}
static unsigned char *b64_decode_ex (const char *src, size_t len, size_t *decsize) {
    int i = 0;
    int j = 0;
    int l = 0;
    size_t size = 0;
    unsigned char *dec = NULL;
    unsigned char buf[3];
    unsigned char tmp[4];
    // alloc
    dec = (unsigned char *) malloc(0);
    if (NULL == dec) { return NULL; }
    // parse until end of source
    while (len--) {
        // break if char is `=' or not base64 char
        if ('=' == src[j]) { break; }
        if (!(isalnum(src[j]) || '+' == src[j] || '/' == src[j])) { break; }
        // read up to 4 bytes at a time into `tmp'
        tmp[i++] = src[j++];
        // if 4 bytes read then decode into `buf'
        if (4 == i) {
            // translate values in `tmp' from table
            for (i = 0; i < 4; ++i) {
                // find translation char in `b64_table'
                for (l = 0; l < 64; ++l) {
                    if (tmp[i] == b64_table[l]) {
                        tmp[i] = l;
                        break;
                    }
                }
            }
            // decode
            buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
            buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
            buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];
            // write decoded buffer to `dec'
            dec = (unsigned char *) realloc(dec, size + 3);
            for (i = 0; i < 3; ++i) {
                dec[size++] = buf[i];
            }
            // reset
            i = 0;
        }
    }
    // remainder
    if (i > 0) {
        // fill `tmp' with `\0' at most 4 times
        for (j = i; j < 4; ++j) {
            tmp[j] = '\0';
        }
        // translate remainder
        for (j = 0; j < 4; ++j) {
            // find translation char in `b64_table'
            for (l = 0; l < 64; ++l) {
                if (tmp[j] == b64_table[l]) {
                    tmp[j] = l;
                    break;
                }
            }
        }
        // decode remainder
        buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
        buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
        buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];
        // write remainer decoded buffer to `dec'
        dec = (unsigned char *) realloc(dec, size + (i - 1));
        for (j = 0; (j < i - 1); ++j) {
            dec[size++] = buf[j];
        }
    }
    // Make sure we have enough space to add '\0' character at end.
    dec = (unsigned char *) realloc(dec, size + 1);
    dec[size] = '\0';
    // Return back the size of decoded string if demanded.
    if (decsize != NULL) *decsize = size;
    return dec;
}
static unsigned char *b64_decode (const char *src, size_t len) {
    return b64_decode_ex(src, len, NULL);
}
/**
 * end b64 functions
 * github.com/littlstar/b64.c
 * MIT license */

/**
 * I found this on a StackOverflow question about TagLib
 * It works so I'm keeping it for now
 * You can set image tags directly from a file.
 * http://stackoverflow.com/a/8467869
 */
#include <tfile.h>
class ImageFileTest : public TagLib::File
{
public:
    ImageFileTest(const char *file) : TagLib::File(file) { }
    TagLib::ByteVector data() { return readBlock(length()); }
private:
    virtual TagLib::Tag *tag() const { return 0; }
    virtual TagLib::AudioProperties *audioProperties() const { return 0; }
    virtual bool save() { return false; }
};

/**
 * making php class constants requires some boilerplate */
#define _defineclassconstant(name, value)           \
    zval * _##name##_ ;                             \
    _##name##_ = (zval *)(pemalloc(sizeof(zval),1));\
    INIT_PZVAL(_##name##_);                         \
    ZVAL_LONG(_##name##_,value);                    \
    zend_hash_add(&ce->constants_table,#name,sizeof(#name),(void *)&_##name##_,sizeof(zval*),NULL);

/**
 * Expose Class Constants from TagLib 
 */
void taglibbase_register_constants(zend_class_entry *ce)
{
    /**
     * see TagLib::MPEG::File::TagTypes in mpegfile.h 
     *
     * For use with TagLibMPEG::stripTags() in this extension 
     */
    _defineclassconstant( STRIP_NOTAGS, 0x0000 );
    _defineclassconstant( STRIP_ID3V1,  0x0001 );
    _defineclassconstant( STRIP_ID3V2,  0x0002 );
    _defineclassconstant( STRIP_APE,    0x0004 );
    _defineclassconstant( STRIP_ALLTAGS,0xffff );

    /**
     * see TagLib::ID3v2::AttachedPictureFrame::Type in attachedpictureframe.h 
     *
     * For use with get/set ID3v2 functions in this extension.
     */
    _defineclassconstant( APIC_OTHER,              0x00);
    _defineclassconstant( APIC_FILEICON,           0x01);
    _defineclassconstant( APIC_OTHERFILEICON,      0x02);
    _defineclassconstant( APIC_FRONTCOVER,         0x03);
    _defineclassconstant( APIC_BACKCOVER,          0x04);
    _defineclassconstant( APIC_LEAFLETPAGE,        0x05);
    _defineclassconstant( APIC_MEDIA,              0x06);
    _defineclassconstant( APIC_LEADARTIST,         0x07);
    _defineclassconstant( APIC_ARTIST,             0x08);
    _defineclassconstant( APIC_CONDUCTOR,          0x09);
    _defineclassconstant( APIC_BAND,               0x0A);
    _defineclassconstant( APIC_COMPOSER,           0x0B);
    _defineclassconstant( APIC_LYRICIST,           0x0C);
    _defineclassconstant( APIC_RECORDINGLOCATION,  0x0D);
    _defineclassconstant( APIC_DURINGRECORDING,    0x0E);
    _defineclassconstant( APIC_DURINGPERFORMANCE,  0x0F);
    _defineclassconstant( APIC_MOVIESCREENCAPTURE, 0x10);
    _defineclassconstant( APIC_COLOUREDFISH,       0x11);
    _defineclassconstant( APIC_ILLUSTRATION,       0x12);
    _defineclassconstant( APIC_BANDLOGO,           0x13);
    _defineclassconstant( APIC_PUBLISHERLOGO,      0x14);
}
static zend_function_entry php_taglibbase_methods[] = {
    { NULL, NULL, NULL }
};

/**
 * more .h files will be included in each of the .cpp files*/
#include "TSRM.h"
#include <tlist.h>
#include <tpropertymap.h>
#include <tstringlist.h>

/**
 * just trying to separate out some of this code */
#include "taglibmpeg.cpp"
#include "taglibogg.cpp"

zend_class_entry *taglibbase_class_entry;
/**
 * And let's try to unify them into one extension 
 * which provides all of the classes */
PHP_MINIT_FUNCTION(taglib_minit)
{
    zend_class_entry base_ce;
    zend_class_entry mpeg_ce;
    zend_class_entry ogg_ce;

    INIT_CLASS_ENTRY(base_ce, "TagLib", php_taglibbase_methods);
    taglibbase_class_entry = zend_register_internal_class(&base_ce TSRMLS_CC);
    taglibbase_register_constants(taglibbase_class_entry);

    INIT_CLASS_ENTRY(mpeg_ce, "TagLibMPEG", php_taglibmpeg_methods);
    taglibmpeg_class_entry = zend_register_internal_class(&mpeg_ce TSRMLS_CC);
//    taglibmpeg_register_constants(taglibmpeg_class_entry);

    taglibmpeg_class_entry->create_object = taglibmpegfile_create_handler;
    memcpy(&taglibmpegfile_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    taglibmpegfile_object_handlers.clone_obj = NULL;

    INIT_CLASS_ENTRY(ogg_ce, "TagLibOGG", php_taglibogg_methods);
    taglibogg_class_entry = zend_register_internal_class(&ogg_ce TSRMLS_CC);
    taglibogg_register_constants(taglibogg_class_entry);

    taglibogg_class_entry->create_object = tagliboggfile_create_handler;
    memcpy(&tagliboggfile_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    tagliboggfile_object_handlers.clone_obj = NULL;

    return SUCCESS;
}


static zend_module_dep php_sample_deps[] = {
    ZEND_MOD_REQUIRED("standard")
    {NULL,NULL,NULL}
};

zend_module_entry taglib_module_entry = {

#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER_EX, NULL,
    php_sample_deps,
//    STANDARD_MODULE_HEADER,
#endif

    PHP_TAGLIB_EXTNAME,
    NULL, /* Functions */
    PHP_MINIT(taglib_minit), /* MINIT */
    NULL, /* MSHUTDOWN */
    NULL, /* RINIT */
    NULL, /* RSHUTDOWN */
    NULL, /* MINFO */

#if ZEND_MODULE_API_NO >= 20010901
    PHP_TAGLIB_EXTVER,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_TAGLIB
ZEND_GET_MODULE(taglib)
#endif
