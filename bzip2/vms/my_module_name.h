/* 2010-11-30 SMS.
 *
 * my_module_name.h
 *
 * DECC "#pragma module" directive to get an upper-case module name
 * from a lower-case source file, to avoid annoying %MMS-W-GWKACTNOUPD
 * or %MMK-I-ACTNOUPD complaints when adding modules to an object
 * library.
 *
 * The C macro, MY_MODULE_NAME, must be defined appropriately.
 */

#ifdef __DECC
# pragma module MY_MODULE_NAME
#endif /* def __DECC */

