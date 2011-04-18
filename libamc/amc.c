#include<amc.h>

int amc_voice()
{
	amc_start(AUDIO_AT_CHANNEL_NAME, NULL);
	amc_disable(AMC_I2S1_RX);
	amc_disable(AMC_I2S2_RX);
	amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_DEFAULT_S);
	amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_DEFAULT_D);
	amc_setGainsource(AMC_I2S1_RX, volumeCTRL.gainMaxDDB);
	amc_setGainsource(AMC_RADIO_RX, volumeCTRL.gainMaxDDB);
	amc_route(AMC_RADIO_RX, AMC_I2S1_TX);
	amc_route(AMC_I2S1_RX, AMC_RADIO_TX);
	amc_enable(AMC_I2S1_RX);
	LOGD("%s:%d \n", __func__, __LINE__);
	return 0;
}

int amc_bt()
{
	amc_start(AUDIO_AT_CHANNEL_NAME, NULL);
	amc_disable(AMC_I2S1_RX);
	amc_disable(AMC_I2S2_RX);
	amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER,  IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_DEFAULT_S);
	amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER,  IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_DEFAULT_D);
	amc_setGainsource(AMC_I2S1_RX, volumeCTRL.gainMaxDDB);
	amc_setGainsource(AMC_RADIO_RX, volumeCTRL.gainMaxDDB);
	amc_route(AMC_RADIO_RX, AMC_I2S1_TX);
	amc_route(AMC_I2S1_RX, AMC_RADIO_TX);
	amc_enable(AMC_I2S1_RX);
	LOGD("%s:%d \n", __func__, __LINE__);
	return 0;
}
