/**
 * the ride never ends
 */
#include <oggfile.h>
#include <oggflacfile.h>
#include <oggpage.h>
#include <oggpageheader.h>
#include <vorbisfile.h>
#include <vorbisproperties.h>
#include <opusfile.h>
#include <opusproperties.h>
#include <flacfile.h>
#include <flacmetadatablock.h>
#include <flacpicture.h>
#include <flacproperties.h>
#include <xiphcomment.h>

/**
 * TagLib handles oggs based on their codec
 * so let's make some unnecessary hoops for us to jump through
 * to abstract it into one TagLibOGG class for PHP rather than three
 */
#define _OGG_VORBIS_ 0x01
#define _OGG_OPUS_ 0x02
#define _OGG_FLAC_ 0x03
#define _OGG_SPEEX_ 0x04

struct tagliboggfile_object {
  zend_object std;
  unsigned long type;
  bool initialized;
  TagLib::Ogg::Vorbis::File *vorbisfile;
  TagLib::Ogg::Opus::File *opusfile;
  TagLib::Ogg::FLAC::File *flacfile;
  TagLib::Ogg::XiphComment *xiphcomment;
};

/**
 * Memory Management
 */
zend_object_handlers tagliboggfile_object_handlers;
void tagliboggfile_free_storage(void *object TSRMLS_DC) {
  tagliboggfile_object *obj = (tagliboggfile_object *)object;

  zend_hash_destroy(obj->std.properties);
  FREE_HASHTABLE(obj->std.properties);

  efree(obj);
}

zend_object_value tagliboggfile_create_handler(zend_class_entry *type
                                                   TSRMLS_DC) {
  zval *tmp;
  zend_object_value retval;

  tagliboggfile_object *obj =
      (tagliboggfile_object *)emalloc(sizeof(tagliboggfile_object));
  memset(obj, 0, sizeof(tagliboggfile_object));
  obj->std.ce = type;

  ALLOC_HASHTABLE(obj->std.properties);
  zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);

#if PHP_VERSION_ID < 50399
  zend_hash_copy(obj->std.properties, &type->default_properties,
                 (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
  object_properties_init((zend_object *)&(obj->std.properties), type);
#endif
  retval.handle = zend_objects_store_put(obj, NULL, tagliboggfile_free_storage,
                                         NULL TSRMLS_CC);
  retval.handlers = &tagliboggfile_object_handlers;

  return retval;
}

/**
 * End Memory Management
 */

/*
 * class entry
 */
zend_class_entry *taglibogg_class_entry;

/**
 * Class Constants
 * _defineclassconstant macro defined in taglib.cpp
 */
void taglibogg_register_constants(zend_class_entry *ce) {
  _defineclassconstant(VORBIS, _OGG_VORBIS_);
  _defineclassconstant(OPUS, _OGG_OPUS_);
  _defineclassconstant(FLAC, _OGG_FLAC_);
  _defineclassconstant(SPEEX, _OGG_SPEEX_);
}

/**
 *  public function __construct( $fileName, $type = TagLibOGG::VORBIS )
 */
PHP_METHOD(TagLibOGG, __construct) {
  zval *fileName;
  long codec = _OGG_VORBIS_;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|l", &fileName,
                            &codec) == FAILURE) {
    php_exception("Expected filename in constructor");
    RETURN_FALSE;
  }

  if (Z_TYPE_P(fileName) != IS_STRING) {
    php_exception("Expected filename in constructor to be a string.");
    RETURN_FALSE;
  }

  tagliboggfile_object *thisobj =
      (tagliboggfile_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

  TagLib::FileName oggFileName = (TagLib::FileName)Z_STRVAL_P(fileName);

  if (!TagLib::File::isReadable(oggFileName)) {
    char msg[sizeof(oggFileName) + 25];
    php_sprintf(msg, "%s cannot be open or read", oggFileName);
    php_exception((const char *)msg);
    RETURN_FALSE;
  }

  if (!TagLib::File::isWritable(oggFileName)) {
    char msg[sizeof(oggFileName) + 22];
    php_sprintf(msg, "%s cannot be written to", oggFileName);
    php_exception((const char *)msg);
    RETURN_FALSE;
  }

  switch (codec) {
  case _OGG_VORBIS_: {
    thisobj->type = _OGG_VORBIS_;
    thisobj->vorbisfile = new TagLib::Ogg::Vorbis::File(oggFileName);
    if (taglib_error() || !thisobj->vorbisfile->isValid()) {
      delete thisobj->vorbisfile;
      char msg[sizeof(oggFileName) + 25];
      php_sprintf(msg, "%s cannot be open or read", oggFileName);
      php_exception((const char *)msg);
      RETURN_FALSE;
    } else {
      thisobj->xiphcomment = thisobj->vorbisfile->tag();
    }
  } break;
  case _OGG_OPUS_: {
    thisobj->type = _OGG_OPUS_;
    thisobj->opusfile = new TagLib::Ogg::Opus::File(oggFileName);
    if (taglib_error() || !thisobj->opusfile->isValid()) {
      delete thisobj->opusfile;
      char msg[sizeof(oggFileName) + 25];
      php_sprintf(msg, "%s cannot be open or read", oggFileName);
      php_exception((const char *)msg);
      RETURN_FALSE;
    } else {
      thisobj->xiphcomment = thisobj->opusfile->tag();
    }
  } break;
  case _OGG_FLAC_: {
    thisobj->type = _OGG_FLAC_;
    thisobj->flacfile = new TagLib::Ogg::FLAC::File(oggFileName);
    if (taglib_error() || !thisobj->flacfile->isValid()) {
      delete thisobj->flacfile;
      char msg[sizeof(oggFileName) + 25];
      php_sprintf(msg, "%s cannot be open or read", oggFileName);
      php_exception((const char *)msg);
      RETURN_FALSE;
    } else {
      thisobj->xiphcomment = thisobj->flacfile->tag();
    }
  } break;
  case _OGG_SPEEX_:
    php_error(E_DEPRECATED, "Speex is deprecated.");
  /* fallthrough */
  default:
    php_exception("Unrecognized or unsupported option for $codec in "
                  "TagLibOGG::__construct()");
    RETURN_FALSE;
  }

  if (taglib_error()) {
    switch (codec) {
    case _OGG_VORBIS_:
      delete thisobj->vorbisfile;
      break;
    case _OGG_OPUS_:
      delete thisobj->opusfile;
      break;
    case _OGG_FLAC_:
      delete thisobj->flacfile;
      break;
    }
    php_exception("taglib returned error");
    RETURN_FALSE;
  }

  thisobj->initialized = true;
}

/**
 *  public function getAudioProperties()
 */
PHP_METHOD(TagLibOGG, getAudioProperties) {
  tagliboggfile_object *thisobj =
      (tagliboggfile_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
  if (!thisobj->initialized) {
    RETURN_FALSE;
  }
  array_init(return_value);
  switch (thisobj->type) {
  case _OGG_VORBIS_: {
    TagLib::Vorbis::Properties *audioProperties =
        thisobj->vorbisfile->audioProperties();

    add_assoc_long(return_value, "length", audioProperties->length());
    add_assoc_long(return_value, "bitrate", audioProperties->bitrate());
    add_assoc_long(return_value, "sampleRate", audioProperties->sampleRate());
    add_assoc_long(return_value, "channels", audioProperties->channels());
    add_assoc_long(return_value, "vorbisVersion",
                   audioProperties->vorbisVersion());
    add_assoc_long(return_value, "bitrateMaximum",
                   audioProperties->bitrateMaximum());
    add_assoc_long(return_value, "bitrateNominal",
                   audioProperties->bitrateNominal());
    add_assoc_long(return_value, "bitrateMinimum",
                   audioProperties->bitrateMinimum());
  } break;
  case _OGG_OPUS_: {
    TagLib::Ogg::Opus::Properties *audioProperties =
        thisobj->opusfile->audioProperties();

    add_assoc_long(return_value, "length", audioProperties->length());
    add_assoc_long(return_value, "bitrate", audioProperties->bitrate());
    add_assoc_long(return_value, "sampleRate", audioProperties->sampleRate());
    add_assoc_long(return_value, "channels", audioProperties->channels());
    add_assoc_long(return_value, "opusVersion", audioProperties->opusVersion());
    add_assoc_long(return_value, "inputSampleRate",
                   audioProperties->inputSampleRate());
  } break;
  case _OGG_FLAC_: {
    TagLib::Ogg::FLAC::Properties *audioProperties =
        thisobj->flacfile->audioProperties();

    add_assoc_long(return_value, "length", audioProperties->length());
    add_assoc_long(return_value, "bitrate", audioProperties->bitrate());
    add_assoc_long(return_value, "sampleRate", audioProperties->sampleRate());
    add_assoc_long(return_value, "channels", audioProperties->channels());
    add_assoc_long(return_value, "sampleWidth", audioProperties->sampleWidth());
    add_assoc_long(return_value, "sampleFrames",
                   audioProperties->sampleFrames());
    //        add_assoc_string(return_value, "signature",
    //        audioProperties->signature().data(), 1);
  } break;
  }
}

/**
 * public function hasXiphComment()
 */
PHP_METHOD(TagLibOGG, hasXiphComment) {
  tagliboggfile_object *thisobj =
      (tagliboggfile_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
  if (!thisobj->initialized) {
    RETURN_FALSE;
  }
  if (thisobj->xiphcomment->fieldCount() > 0) {
    RETURN_TRUE;
  } else {
    RETURN_FALSE;
  }
}

/**
 * public function getXiphComment()
 */
PHP_METHOD(TagLibOGG, getXiphComment) {
  tagliboggfile_object *thisobj =
      (tagliboggfile_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
  if (!thisobj->initialized) {
    RETURN_FALSE;
  }

  array_init(return_value);

  TagLib::PropertyMap propMap = thisobj->xiphcomment->properties();

  for (TagLib::Map<TagLib::String, TagLib::StringList>::Iterator property =
           propMap.begin();
       property != propMap.end(); property++) {
    add_assoc_string(return_value, property->first.toCString(),
                     (char *)(property->second.toString().toCString()), 1);
  }
}

PHP_METHOD(TagLibOGG, stripTags) {
  tagliboggfile_object *thisobj =
      (tagliboggfile_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
  if (!thisobj->initialized) {
    RETURN_FALSE;
  }

  TagLib::PropertyMap propMap = thisobj->xiphcomment->properties();

  int i = 0;
  int size = propMap.size();
  TagLib::String keys[size];
  for (TagLib::Map<TagLib::String, TagLib::StringList>::Iterator property =
           propMap.begin();
       property != propMap.end(); property++) {
    keys[i] = property->first;
    i++;
  }
  for (i = 0; i < size; i++) {
    propMap.erase(keys[i]);
  }
  thisobj->xiphcomment->setProperties(propMap);
  switch (thisobj->type) {
  case _OGG_VORBIS_:
    if (thisobj->vorbisfile->save()) {
      RETURN_TRUE;
      return;
    }
    break;
  case _OGG_OPUS_:
    if (thisobj->opusfile->save()) {
      RETURN_TRUE;
      return;
    }
    break;
  case _OGG_FLAC_:
    if (thisobj->flacfile->save()) {
      RETURN_TRUE;
      return;
    }
    break;
  }
  RETURN_FALSE;
}

/**
 * returns FALSE if something went wrong,
 * returns TRUE if everything went through */
PHP_METHOD(TagLibOGG, setXiphComment) {
  tagliboggfile_object *thisobj =
      (tagliboggfile_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
  if (!thisobj->initialized) {
    RETURN_FALSE;
  }

  zval *newProperties;
  zend_bool overwrite_existing_tags = true;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|b", &newProperties,
                            &overwrite_existing_tags) == FAILURE) {
    RETURN_FALSE;
  }

  if (Z_TYPE_P(newProperties) != IS_ARRAY) {
    php_error(
        E_WARNING,
        "TagLibOGG::setXiphComment expects associative array of strings!");
    RETURN_FALSE;
  }

  HashTable *hIndex = Z_ARRVAL_P(newProperties);
  HashPosition pointer;
  zval **data;
  for (zend_hash_internal_pointer_reset_ex(hIndex, &pointer);
       zend_hash_get_current_data_ex(hIndex, (void **)&data, &pointer) ==
           SUCCESS;
       zend_hash_move_forward_ex(hIndex, &pointer)) {
    char *key;
    uint key_length, key_type;
    ulong index;
    key_type = zend_hash_get_current_key_ex(hIndex, &key, &key_length, &index,
                                            0, &pointer);

    if (key_type != HASH_KEY_IS_STRING) {
      php_error(
          E_WARNING,
          "TagLibOGG::setXiphComment expects associative array of strings!");
      RETURN_FALSE;
      return;
    }

    if (Z_TYPE_PP(data) != IS_STRING) {
      php_error(
          E_WARNING,
          "TagLibOGG::setXiphComment expects associative array of strings!");
      RETURN_FALSE;
      return;
    }

    TagLib::String *destKey = new TagLib::String((const char *)key);
    TagLib::String *destValue = new TagLib::String(Z_STRVAL_PP(data));

    thisobj->xiphcomment->addField(*destKey, *destValue,
                                   (bool)overwrite_existing_tags);
    if (taglib_error()) {
      php_error(E_WARNING, "XiphComment::addField() failed.");
      return;
    }
  }

  switch (thisobj->type) {
  case _OGG_VORBIS_:
    if (thisobj->vorbisfile->save()) {
      RETURN_TRUE;
      return;
    }
    break;
  case _OGG_OPUS_:
    if (thisobj->opusfile->save()) {
      RETURN_TRUE;
      return;
    }
    break;
  case _OGG_FLAC_:
    if (thisobj->flacfile->save()) {
      RETURN_TRUE;
      return;
    }
    break;
  }

  taglib_error();
  RETURN_FALSE;
}

/**
 * Now we assemble the above defined methods into the class or something
 */
static zend_function_entry php_taglibogg_methods[] = {
    PHP_ME(TagLibOGG, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(TagLibOGG, getAudioProperties, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(TagLibOGG, hasXiphComment, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(TagLibOGG, getXiphComment, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(TagLibOGG, setXiphComment, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(TagLibOGG, stripTags, NULL, ZEND_ACC_PUBLIC){NULL, NULL, NULL}};
