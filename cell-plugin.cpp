#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("cell-plugin", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "cell plugin source/output";
}

extern void RegisterDShowSource();

bool obs_module_load(void)
{
	RegisterDShowSource();
	return true;
}
