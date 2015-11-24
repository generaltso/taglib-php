/**
 * this is the future you asked for 
 */
//#include <.h>
struct taglibflacfile_object {
    zend_object std;
    unsigned long type;
    bool initialized;
    TagLib::FLAC::File *file;
    TagLib::ID3v2::FrameFactory *frameFactory;
    TagLib::Ogg::XiphComment *xiphcomment;
};

/**
 * Memory Management
 */
zend_object_handlers taglibflacfile_object_handlers;
void taglibflacfile_free_storage(void *object TSRMLS_DC) {
    taglibflacfile_object *obj = (taglibflacfile_object *)object;
    delete obj->file;

    zend_hash_destroy(obj->std.properties);
    FREE_HASHTABLE(obj->std.properties);

    efree(obj);
}

zend_object_value taglibflacfile_create_handler(zend_class_entry *type TSRMLS_DC) {
    zval *tmp;
    zend_object_value retval;

    taglibflacfile_object *obj = (taglibflacfile_object *) emalloc(sizeof(taglibflacfile_object));
    memset(obj, 0, sizeof(taglibflacfile_object));
    obj->std.ce = type;

    ALLOC_HASHTABLE(obj->std.properties);
    zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
    
#if PHP_VERSION_ID < 50399
    zend_hash_copy(obj->std.properties, &type->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#else
    object_properties_init((zend_object *) &(obj->std.properties), type);
#endif 
    retval.handle = zend_objects_store_put(obj, NULL, taglibflacfile_free_storage, NULL TSRMLS_CC);
    retval.handlers = &taglibflacfile_object_handlers;

    return retval;
}

/**
 * End Memory Management
 */

/*
 * class entry
 */
zend_class_entry *taglibflac_class_entry;

/**
 * Class Constants
 * _defineclassconstant macro defined in taglib.cpp
 */
void taglibflac_register_constants(zend_class_entry *ce) {
//    _defineclassconstant( CONSTANT, "value" );
}

/**
 *  public function __construct( $fileName ) 
 */
PHP_METHOD(TagLibFLAC, __construct) {
    zval *fileName;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &fileName) == FAILURE) {
        php_exception("Expected filename in constructor");
        RETURN_FALSE;
    }

    if(Z_TYPE_P(fileName) != IS_STRING) {
        php_exception("Expected filename in constructor to be a string.");
        RETURN_FALSE;
    }

    const char* filestr = Z_STRVAL_P(fileName);

    if(!TagLib::File::isReadable(filestr)) {
        char msg[sizeof(filestr)+25];
        php_sprintf(msg, "%s cannot be open or read", filestr);
        php_exception((const char*)msg);
        RETURN_FALSE;
    }

    if(!TagLib::File::isWritable(filestr)) {
        char msg[sizeof(filestr)+22];
        php_sprintf(msg, "%s cannot be written to", filestr);
        php_exception((const char*)msg);
        RETURN_FALSE;
    }

    taglibflacfile_object *thisobj = (taglibflacfile_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

    thisobj->frameFactory = TagLib::ID3v2::FrameFactory::instance();

    try {
        thisobj->file = new TagLib::FLAC::File(
            (TagLib::FileName) filestr,
            thisobj->frameFactory,
            true,
            TagLib::AudioProperties::ReadStyle::Accurate
        );
    } catch(std::exception& e) {
        php_error(E_WARNING, "%s", e.what());
        php_exception(e.what());
        RETURN_FALSE;
    }    

    if(!thisobj->file->isValid()) {
        char msg[sizeof(filestr)+24];
        php_sprintf(msg, "%s cannot be open or read", filestr);
        php_exception((const char*)msg);
        RETURN_FALSE;
    }

    thisobj->xiphcomment = thisobj->file->xiphComment();

    if(taglib_error()) {
        delete thisobj->file;
        php_exception("taglib returned error");
        RETURN_FALSE;
    }

    thisobj->initialized = true;
}

/**
 *  public function getAudioProperties()
 */
PHP_METHOD(TagLibFLAC, getAudioProperties) {
    taglibflacfile_object *thisobj = (taglibflacfile_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
    if(!thisobj->initialized) {
        RETURN_FALSE;
    }
    array_init(return_value);

    TagLib::FLAC::Properties *audioProperties = thisobj->file->audioProperties();

    add_assoc_long(return_value, "length", audioProperties->length());
    add_assoc_long(return_value, "bitrate", audioProperties->bitrate());
    add_assoc_long(return_value, "sampleRate", audioProperties->sampleRate());
    add_assoc_long(return_value, "channels", audioProperties->channels());
    add_assoc_long(return_value, "sampleWidth", audioProperties->sampleWidth());
    add_assoc_long(return_value, "sampleFrames", audioProperties->sampleFrames());

    // XXX: broken, can't figure out how to read this data or what it means, or why I should care about it
    // using md5sum or sha1sum on the file itself to verify data integrity or whatever is preferable in any case
    // and totally outside the scope of tagging imho tbh fam
    //add_assoc_string(return_value, "signature", audioProperties->signature().toHex().data(), 1);
}

/**
 * public function hasXiphComment()
 */
PHP_METHOD(TagLibFLAC, hasXiphComment) {
    taglibflacfile_object *thisobj = (taglibflacfile_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
    if(!thisobj->initialized) {
        RETURN_FALSE;
    }
    if(thisobj->xiphcomment->fieldCount() > 0) {
        RETURN_TRUE;
    } else {
        RETURN_FALSE;
    }
}

/**
 * public function getXiphComment()
 */
PHP_METHOD(TagLibFLAC, getXiphComment) {
    taglibflacfile_object *thisobj = (taglibflacfile_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
    if(!thisobj->initialized) {
        RETURN_FALSE;
    }

    array_init(return_value);

    TagLib::PropertyMap propMap = thisobj->xiphcomment->properties();

    for(TagLib::Map<TagLib::String,TagLib::StringList>::Iterator property = propMap.begin(); property != propMap.end(); property++) {
        add_assoc_string(return_value, property->first.toCString(), (char *)(property->second.toString().toCString()), 1);
    }
}

/**
 * returns FALSE if something went wrong,
 * returns TRUE if everything went through */
PHP_METHOD(TagLibFLAC, setXiphComment) {
    taglibflacfile_object *thisobj = (taglibflacfile_object *) zend_object_store_get_object(getThis() TSRMLS_CC);
    if(!thisobj->initialized) {
        RETURN_FALSE;
    }

    zval *newProperties;
    zend_bool overwrite_existing_tags = false;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|b", &newProperties, &overwrite_existing_tags) == FAILURE) {
        RETURN_FALSE;
    }

    if(Z_TYPE_P(newProperties) != IS_ARRAY) {
        php_error(E_WARNING, "TagLibFLAC::setXiphComment expects associative array of strings!");
        RETURN_FALSE;
    }
    
    HashTable *hIndex = Z_ARRVAL_P(newProperties);
    HashPosition pointer;
    zval **data;
    for(zend_hash_internal_pointer_reset_ex(hIndex, &pointer);
        zend_hash_get_current_data_ex(hIndex, (void**)&data, &pointer) == SUCCESS;
        zend_hash_move_forward_ex(hIndex, &pointer))
    {
        char *key;
        uint key_length, key_type;
        ulong index;
        key_type = zend_hash_get_current_key_ex(hIndex, &key, &key_length, &index, 0, &pointer);

        if(key_type != HASH_KEY_IS_STRING) {
            php_error(E_WARNING, "TagLibFLAC::setXiphComment expects associative array of strings!");
            RETURN_FALSE;
            return;
        }

        if(Z_TYPE_PP(data) != IS_STRING) {
            php_error(E_WARNING, "TagLibFLAC::setXiphComment expects associative array of strings!");
            RETURN_FALSE;
            return;
        }

        TagLib::String *destKey   = new TagLib::String((const char *)key);
        TagLib::String *destValue = new TagLib::String(Z_STRVAL_PP(data));

        thisobj->xiphcomment->addField(*destKey,*destValue,(bool)overwrite_existing_tags);
        if(taglib_error()) {
            php_error(E_WARNING, "XiphComment::addField() failed.");
            return;
        }
    }

    if(thisobj->file->save()) {
        RETURN_TRUE;
        return;
    }

    taglib_error();
    RETURN_FALSE;
}

/**
 * Now we assemble the above defined methods into the class or something
 */
static zend_function_entry php_taglibflac_methods[] = {
    PHP_ME(TagLibFLAC, __construct,         NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(TagLibFLAC, getAudioProperties,  NULL, ZEND_ACC_PUBLIC)
    PHP_ME(TagLibFLAC, hasXiphComment,      NULL, ZEND_ACC_PUBLIC)
    PHP_ME(TagLibFLAC, getXiphComment,      NULL, ZEND_ACC_PUBLIC)
    PHP_ME(TagLibFLAC, setXiphComment,      NULL, ZEND_ACC_PUBLIC)
    { NULL, NULL, NULL }
};
