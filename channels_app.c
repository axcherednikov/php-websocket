#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_websocket.h"
#include "php_websocket_compat.h"
#include "websocket_arginfo.h"

static zend_object_handlers channels_app_handlers;

channels_app_object *channels_app_from_obj(zend_object *obj)
{
	return (channels_app_object *)((char *)(obj) - XtOffsetOf(channels_app_object, std));
}

static zend_object *channels_app_create_object(zend_class_entry *ce)
{
	channels_app_object *intern = zend_object_alloc(sizeof(channels_app_object), ce);

	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);

	intern->key = NULL;
	intern->secret = NULL;
	intern->id = NULL;
	ZVAL_UNDEF(&intern->options);

	intern->std.handlers = &channels_app_handlers;

	return &intern->std;
}

static void channels_app_free_object(zend_object *object)
{
	channels_app_object *intern = channels_app_from_obj(object);

	if (intern->key) {
		zend_string_release(intern->key);
	}
	if (intern->secret) {
		zend_string_release(intern->secret);
	}
	if (intern->id) {
		zend_string_release(intern->id);
	}
	zval_ptr_dtor(&intern->options);

	zend_object_std_dtor(&intern->std);
}

PHP_METHOD(Channels_App, __construct)
{
	zend_string *key;
	zend_string *secret;
	zend_string *id;
	zval *options = NULL;
	channels_app_object *intern = Z_CHANNELS_APP_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_START(3, 4)
		Z_PARAM_STR(key)
		Z_PARAM_STR(secret)
		Z_PARAM_STR(id)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(options)
	ZEND_PARSE_PARAMETERS_END();

	intern->key = zend_string_copy(key);
	intern->secret = zend_string_copy(secret);
	intern->id = zend_string_copy(id);

	if (options) {
		ZVAL_COPY(&intern->options, options);
	} else {
		array_init(&intern->options);
	}
}

void websocket_register_channels_app_class(void)
{
	channels_app_ce = register_class_Channels_App();
	channels_app_ce->create_object = channels_app_create_object;

	memcpy(&channels_app_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	channels_app_handlers.offset = XtOffsetOf(channels_app_object, std);
	channels_app_handlers.free_obj = channels_app_free_object;
	channels_app_handlers.clone_obj = NULL;
	WEBSOCKET_SET_DEFAULT_HANDLERS(channels_app_ce, &channels_app_handlers);
}
