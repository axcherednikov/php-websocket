/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */

#ifndef PHP_WEBSOCKET_COMPAT_H
#define PHP_WEBSOCKET_COMPAT_H

#include "php.h"
#include "zend_API.h"

/*
 * Compatibility shims for PHP 8.1 – 8.5+.
 *
 * The public API targets PHP >= 8.1 because it uses enums and readonly
 * properties. Internal Zend helpers differ between minor releases, so the C
 * code uses the newer spelling and keeps the fallback here.
 */

#if PHP_VERSION_ID < 80400
static inline zend_class_entry *zend_register_internal_class_with_flags(
	zend_class_entry *ce, zend_class_entry *parent, uint32_t flags)
{
	zend_class_entry *registered = zend_register_internal_class_ex(ce, parent);
	registered->ce_flags |= flags;
	return registered;
}
#endif

#if PHP_VERSION_ID < 80300
# define zend_declare_typed_class_constant(ce, name, value, access_type, attributes, type) \
	zend_declare_class_constant((ce), ZSTR_VAL(name), ZSTR_LEN(name), (value))
#endif

#if PHP_VERSION_ID >= 80300
# define WEBSOCKET_SET_DEFAULT_HANDLERS(ce, h) \
	(ce)->default_object_handlers = (h)
#else
# define WEBSOCKET_SET_DEFAULT_HANDLERS(ce, h) \
	/* not available before PHP 8.3 */
#endif

static inline const char *websocket_zval_value_name(const zval *value)
{
#if PHP_VERSION_ID >= 80300
	return zend_zval_value_name(value);
#else
	return zend_zval_type_name(value);
#endif
}

static inline void websocket_call_known_fcc(const zend_fcall_info_cache *fcc, zval *retval_ptr, uint32_t param_count, zval *params)
{
	zend_call_known_function(fcc->function_handler, fcc->object, fcc->called_scope, retval_ptr, param_count, params, NULL);
}

#endif /* PHP_WEBSOCKET_COMPAT_H */
