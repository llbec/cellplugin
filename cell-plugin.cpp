#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("cell-plugin", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "cell plugin source/output";
}

extern void RegisterCellAirSource();
extern void RegisterCamOutput();

bool obs_module_load(void)
{
	RegisterCellAirSource();
	RegisterCamOutput();

	/*obs_data_t *obs_settings = obs_data_create();
	obs_data_set_bool(obs_settings, "vcamEnabled", true);
	obs_apply_private_data(obs_settings);
	obs_data_release(obs_settings);*/

	return true;
}
