/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <cutils/log.h>
#include <cutils/atomic.h>

#include <IntelHWComposer.h>
#include <IntelOverlayUtil.h>
#include <IntelWidiPlane.h>
#include <IntelHWComposerCfg.h>

IntelHWComposer::~IntelHWComposer()
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    delete mPlaneManager;
    delete mBufferManager;
    delete mGrallocBufferManager;
    delete mDrm;

    for (size_t i=0; i<DISPLAY_NUM; i++) {
        delete mDisplayDevice[i];
     }
    // stop uevent observer
    stopObserver();
}

void IntelHWComposer::signalHpdCompletion()
{
    Mutex::Autolock _l(mHpdLock);
    if (mHpdCompletion == false) {
        mHpdCompletion = true;
        mHpdCondition.signal();
        ALOGD("%s: send out hpd completion signal\n", __func__);
    }
}

void IntelHWComposer::waitForHpdCompletion()
{
    Mutex::Autolock _l(mHpdLock);

    // time out for 300ms
    nsecs_t reltime = 300000000;
    mHpdCompletion = false;
    while(!mHpdCompletion) {
        mHpdCondition.waitRelative(mHpdLock, reltime);
    }

    ALOGD("%s: receive hpd completion signal: %d\n", __func__, mHpdCompletion?1:0);
}

bool IntelHWComposer::handleDisplayModeChange()
{
    android::Mutex::Autolock _l(mLock);
    ALOGD_IF(ALLOW_HWC_PRINT, "handleDisplayModeChange");

    if (!mDrm) {
         ALOGW("%s: mDrm is not intilized!\n", __func__);
         return false;
    }

    mDrm->detectMDSModeChange();

    for (size_t i=0; i<DISPLAY_NUM; i++)
        mDisplayDevice[i]->onHotplugEvent(true);

    return true;
}

bool IntelHWComposer::handleHotplugEvent(int hpd, void *data, int *modeIndex)
{
    bool ret = false;
    int disp = 1;

    ALOGD_IF(ALLOW_HWC_PRINT, "handleHotplugEvent");

    if (!mDrm) {
        ALOGW("%s: mDrm is not intialized!\n", __func__);
        goto out;
    }

    if (hpd) {
        // get display mode
        intel_display_mode_t *s_mode = (intel_display_mode_t *)data;
        drmModeModeInfoPtr mode;
        mode = mDrm->selectDisplayDrmMode(disp, s_mode, modeIndex);
        if (!mode) {
            ret = false;
            goto out;
        }

        // alloc buffer;
        mHDMIFBHandle.size = align_to(mode->vdisplay * mode->hdisplay * 4, 64);
        ret = mGrallocBufferManager->alloc(mHDMIFBHandle.size,
                                      &mHDMIFBHandle.umhandle,
                                      &mHDMIFBHandle.kmhandle);
        if (!ret)
            goto out;

        // mode setting;
        ret = mDrm->setDisplayDrmMode(disp, mHDMIFBHandle.kmhandle, mode);
        if (!ret)
            goto out;
    } else {
        // rm FB
        ret = mDrm->handleDisplayDisConnection(disp);
        if (!ret)
            goto out;

        // release buffer;
        ret = mGrallocBufferManager->dealloc(mHDMIFBHandle.umhandle);
        if (!ret)
            goto out;

        memset(&mHDMIFBHandle, 0, sizeof(mHDMIFBHandle));
    }

out:
    if (ret) {
        ALOGD("%s: detected hdmi hotplug event:%s\n", __func__, hpd?"IN":"OUT");
        handleDisplayModeChange();

        /* hwc_dev->procs is set right after the device is opened, but there is
         * still a race condition where a hotplug event might occur after the open
         * but before the procs are registered. */
        if (mProcs && mProcs->vsync) {
            mProcs->hotplug(mProcs, HWC_DISPLAY_EXTERNAL, hpd);
        }
    }

    return ret;
}

bool IntelHWComposer::handleDynamicModeSetting(void *data, int* modeIndex)
{
    bool ret = false;

    ALOGD_IF(ALLOW_HWC_PRINT, "%s: handle Dynamic mode setting!\n", __func__);
    // send plug-out to SF for mode changing on the same device
    // otherwise SF will bypass the plug-in message as there is
    // no connection change;
    ret = handleHotplugEvent(0, NULL, NULL);
    if (!ret) {
        ALOGW("%s: send fake unplug event failed!\n", __func__);
        goto out;
    }

    // TODO: here we need to wait for the plug-out take effect.
    waitForHpdCompletion();

    // then change the mode and send plug-in to SF
    ret = handleHotplugEvent(1, data, modeIndex);
    if (!ret) {
        ALOGW("%s: send plug in event failed!\n", __func__);
        goto out;
    }
out:
    return ret;
}

bool IntelHWComposer::onUEvent(const char *msg, int msgLen, int msgType, void *data, int* modeIndex)
{
    bool ret = false;
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    // if mds sent orientation change message, inform widi plane and return
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_ORIENTATION_CHANGE) {
        ALOGD("%s: got multiDisplay service orientation change event\n", __func__);
        if(mPlaneManager->isWidiActive()) {
            IntelWidiPlane* widiPlane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();
            if (widiPlane->setOrientationChanged() != NO_ERROR) {
                ALOGE("%s: error in sending orientation change event to widiplane", __func__);
            }
        }
    }

    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS)
        ret = handleDisplayModeChange();

    // handle hdmi plug in;
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_HOTPLUG_IN)
        ret = handleHotplugEvent(1, data, modeIndex);

    // handle hdmi plug out;
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_HOTPLUG_OUT)
        ret = handleHotplugEvent(0, NULL, NULL);

    // handle dynamic mode setting
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_TIMING_DYNAMIC_SETTING)
        ret = handleDynamicModeSetting(data, modeIndex);

    return ret;
#endif

    if (strcmp(msg, "change@/devices/pci0000:00/0000:00:02.0/drm/card0"))
        return true;
    msg += strlen(msg) + 1;

    do {
        if (!strncmp(msg, "HOTPLUG_IN=1", strlen("HOTPLUG_IN=1"))) {
            ALOGD("%s: detected hdmi hotplug IN event:%s\n", __func__, msg);
            ret = handleHotplugEvent(1, NULL, NULL);
            break;
        } else if (!strncmp(msg, "HOTPLUG_OUT=1", strlen("HOTPLUG_OUT=1"))) {
            ALOGD("%s: detected hdmi hotplug OUT event:%s\n", __func__, msg);
            ret = handleHotplugEvent(0, NULL, NULL);
            break;
        }

        msg += strlen(msg) + 1;
    } while (*msg);

    return ret;
}

void IntelHWComposer::vsync(int64_t timestamp, int pipe)
{
    if (mProcs && mProcs->vsync) {
        ALOGV("%s: report vsync timestamp %llu, pipe %d, active 0x%x", __func__,
             timestamp, pipe, mActiveVsyncs);
        if ((1 << pipe) & mActiveVsyncs)
            mProcs->vsync(const_cast<hwc_procs_t*>(mProcs), 0, timestamp);
    }
    mLastVsync = timestamp;
}

uint32_t IntelHWComposer::disableUnusedVsyncs(uint32_t target)
{
    uint32_t unusedVsyncs = mActiveVsyncs & (~target);
    struct drm_psb_vsync_set_arg arg;
    uint32_t vsync;
    int i, ret;

    ALOGV("disableVsync: unusedVsyncs 0x%x\n", unusedVsyncs);

    if (!unusedVsyncs)
        goto disable_out;

    /*disable unused vsyncs*/
    for (i = 0; i < VSYNC_SRC_NUM; i++) {
        vsync = (1 << i);
        if (!(vsync & unusedVsyncs))
            continue;

        /*disable vsync*/
        if (i == VSYNC_SRC_FAKE)
            mFakeVsync->setEnabled(false, mLastVsync);
        else {
            memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));
            arg.vsync_operation_mask = VSYNC_DISABLE | GET_VSYNC_COUNT;

            // pipe select
            if (i == VSYNC_SRC_HDMI)
                arg.vsync.pipe = 1;
            else
                arg.vsync.pipe = 0;

            ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_VSYNC_SET,
                                      &arg, sizeof(arg));
            if (ret) {
                ALOGW("%s: failed to enable/disable vsync %d\n", __func__, ret);
                continue;
            }
            mVsyncsEnabled = 0;
            mVsyncsCount = arg.vsync.vsync_count;
            mVsyncsTimestamp = arg.vsync.timestamp;
        }

        /*disabled successfully, remove it from unused vsyncs*/
        unusedVsyncs &= ~vsync;
    }
disable_out:
    return unusedVsyncs;
}

uint32_t IntelHWComposer::enableVsyncs(uint32_t target)
{
    uint32_t enabledVsyncs = 0;
    struct drm_psb_vsync_set_arg arg;
    uint32_t vsync;
    int i, ret;

    ALOGV("enableVsyn: enable vsyncs 0x%x\n", target);

    if (!target) {
        enabledVsyncs = 0;
        goto enable_out;
    }

    // remove all active vsyncs from target
    target &= ~mActiveVsyncs;
    if (!target) {
        enabledVsyncs = mActiveVsyncs;
        goto enable_out;
    }

    // enable vsyncs which is currently inactive
    for (i = 0; i < VSYNC_SRC_NUM; i++) {
        vsync = (1 << i);
        if (!(vsync & target))
            continue;

        /*enable vsync*/
        if (i == VSYNC_SRC_FAKE)
            mFakeVsync->setEnabled(true, mLastVsync);
        else {
            memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));
            arg.vsync_operation_mask = VSYNC_ENABLE | GET_VSYNC_COUNT;

            // pipe select
            if (i == VSYNC_SRC_HDMI)
                arg.vsync.pipe = 1;
            else
                arg.vsync.pipe = 0;

            ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_VSYNC_SET,
                                      &arg, sizeof(arg));
            if (ret) {
                ALOGW("%s: failed to enable vsync %d\n", __func__, ret);
                continue;
            }
            mVsyncsEnabled = 1;
            mVsyncsCount = arg.vsync.vsync_count;
            mVsyncsTimestamp = arg.vsync.timestamp;
        }

        /*enabled successfully*/
        enabledVsyncs |= vsync;
    }
enable_out:
    return enabledVsyncs;
}

bool IntelHWComposer::vsyncControl(int enabled)
{
    uint32_t targetVsyncs = 0;
    uint32_t activeVsyncs = 0;
    uint32_t enabledVsyncs = 0;
    IntelWidiPlane* widiPlane = 0;

    ALOGV("vsyncControl, enabled %d\n", enabled);

    if (enabled != 0 && enabled != 1)
        return false;

    android::Mutex::Autolock _l(mLock);

    // for disable vsync request, disable all active vsyncs
    if (!enabled) {
        targetVsyncs = 0;
        goto disable_vsyncs;
    }

    // use fake vsync for widi extend video mode
    widiPlane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();
    if (widiPlane && widiPlane->isActive() &&
        widiPlane->isExtVideoAllowed() &&
        widiPlane->isPlayerOn()) {
        targetVsyncs |= (1 << VSYNC_SRC_FAKE);
    } else if (OVERLAY_EXTEND == mDrm->getDisplayMode()) {
        targetVsyncs |= (1 << VSYNC_SRC_HDMI);
    } else
        targetVsyncs |= (1 << VSYNC_SRC_MIPI);

    // enable selected vsyncs
    enabledVsyncs = enableVsyncs(targetVsyncs);

disable_vsyncs:
    // disable unused vsyncs
    activeVsyncs = disableUnusedVsyncs(targetVsyncs);

    // update active vsyncs
    mActiveVsyncs = enabledVsyncs | activeVsyncs;
    mVsync->setActiveVsyncs(mActiveVsyncs);

    ALOGV("vsyncControl: activeVsyncs 0x%x\n", mActiveVsyncs);
    return true;
}

bool IntelHWComposer::release()
{
    ALOGD("release");

    if (!initCheck())
        return false;

    // disable all devices
    for (size_t i=0 ; i<DISPLAY_NUM ; i++) {
         mDisplayDevice[i]->release();
    }

    return true;
}

bool IntelHWComposer::dumpDisplayStat()
{
    struct drm_psb_register_rw_arg arg;
    struct drm_psb_vsync_set_arg vsync_arg;
    int ret;
#if 0
    // dump vsync info
    memset(&vsync_arg, 0, sizeof(struct drm_psb_vsync_set_arg));
    vsync_arg.vsync_operation_mask = GET_VSYNC_COUNT;
    vsync_arg.vsync.pipe = 0;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_VSYNC_SET,
                               &vsync_arg, sizeof(vsync_arg));
    if (ret) {
        ALOGW("%s: failed to dump vsync info %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("-------------Display Stat -------------------\n");
    dumpPrintf("  + last vsync count: %d, timestamp %d ms \n",
                     mVsyncsCount, mVsyncsTimestamp/1000000);
    dumpPrintf("  + current vsync count: %d, timestamp %d ms \n",
                     vsync_arg.vsync.vsync_count,
                     vsync_arg.vsync.timestamp/1000000);

    // Read pipe stat register
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_PIPEASTAT;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("  + PIPEA STAT: 0x%x \n", arg.display.pipestat_a);

    // Read interrupt mask register
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_INT_MASK;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("  + INT_MASK_REG: 0x%x \n", arg.display.int_mask);

    // Read interrupt enable register
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_INT_ENABLE;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("  + INT_ENABLE_REG: 0x%x \n", arg.display.int_enable);

    // open this if need to dump all display registers.
#if 0
    // dump all display regs in driver
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_DISPLAY_ALL;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }
#endif

out:
    return (ret == 0) ? true : false;
#endif
    return true;
}

bool IntelHWComposer::dump(char *buff,
                           int buff_len, int *cur_len)
{
    IntelDisplayPlane *plane = NULL;
    bool ret = true;
    int i;

    mDumpBuf = buff;
    mDumpBuflen = buff_len;
    mDumpLen = 0;

    dumpDisplayStat();

    for (size_t i=0 ; i<DISPLAY_NUM ; i++) {
         mDisplayDevice[i]->dump(mDumpBuf,  mDumpBuflen, &mDumpLen);
    }

    mPlaneManager->dump(mDumpBuf,  mDumpBuflen, &mDumpLen);

    return ret;
}

bool IntelHWComposer::initialize()
{
    bool ret = true;

    //TODO: replace the hard code buffer type later
    int bufferType = IntelBufferManager::TTM_BUFFER;

    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    // open IMG frame buffer device
    hw_module_t const* module;
    IMG_gralloc_module_public_t *imgGrallocModule;

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0) {
        imgGrallocModule = (IMG_gralloc_module_public_t*)module;
        mFBDev = imgGrallocModule->psFrameBufferDevice;
        mFBDev->bBypassPost = 1; //cfg.bypasspost;
    }

    framebuffer_open(module, (framebuffer_device_t**)&mFBDev);

    if (!mFBDev) {
        ALOGE("%s: failed to open IMG FB device\n", __func__);
        return false;
    }

    //create new DRM object if not exists
    if (!mDrm) {
        mDrm = &IntelHWComposerDrm::getInstance();
        if (!mDrm) {
            ALOGE("%s: Invalid DRM object\n", __func__);
            ret = false;
            goto drm_err;
        }

        ret = mDrm->initialize(this);
        if (ret == false) {
            ALOGE("%s: failed to initialize DRM instance\n", __func__);
            goto drm_err;
        }
    }

    mVsync = new IntelVsyncEventHandler(this, mDrm->getDrmFd());

    mFakeVsync = new IntelFakeVsyncEvent(this);

    // create buffer manager for gralloc buffer
    if (!mGrallocBufferManager) {
        //mGrallocBufferManager = new IntelPVRBufferManager(mDrm->getDrmFd());
        mGrallocBufferManager = new IntelGraphicBufferManager(mDrm->getDrmFd());
        if (!mGrallocBufferManager) {
            ALOGE("%s: Failed to create Gralloc buffer manager\n", __func__);
            ret = false;
            goto gralloc_bm_err;
        }

        ret = mGrallocBufferManager->initialize();
        if (ret == false) {
            ALOGE("%s: Failed to initialize Gralloc buffer manager\n", __func__);
            goto gralloc_bm_err;
        }
    }

    // create new display plane manager
    if (!mPlaneManager) {
        mPlaneManager =
            new IntelDisplayPlaneManager(mDrm->getDrmFd(),
                                         mBufferManager, mGrallocBufferManager);
        if (!mPlaneManager) {
            ALOGE("%s: Failed to create plane manager\n", __func__);
            goto bm_init_err;
        }
    }

     // create display devices
    memset(mDisplayDevice, 0, sizeof(mDisplayDevice));
    for (size_t i=0; i<DISPLAY_NUM; i++) {
         if (i == HWC_DISPLAY_PRIMARY)
             mDisplayDevice[i] =
                 new IntelMIPIDisplayDevice(mBufferManager, mGrallocBufferManager,
                                       mPlaneManager, mFBDev, mDrm, i);
         else if (i == HWC_DISPLAY_EXTERNAL)
             mDisplayDevice[i] =
                 new IntelHDMIDisplayDevice(mBufferManager, mGrallocBufferManager,
                                       mPlaneManager, mFBDev, mDrm, i);
         else
             break;

         if (!mDisplayDevice[i]) {
             ALOGE("%s: Failed to create plane manager\n", __func__);
             goto bm_init_err;
         }
    }

    // init mHDMIBuffers
    memset(&mHDMIFBHandle, 0, sizeof(mHDMIFBHandle));

    // do mode setting in HWC if HDMI is connected when boot up
    if (mDrm->detectDisplayConnection(OUTPUT_HDMI))
        handleHotplugEvent(1, NULL, NULL);

    startObserver();

    mInitialized = true;

    ALOGD_IF(ALLOW_HWC_PRINT, "%s: successfully\n", __func__);
    return true;

pm_err:
    delete mPlaneManager;
bm_init_err:
    delete mGrallocBufferManager;
gralloc_bm_err:
    delete mBufferManager;
    mBufferManager = 0;
bm_err:
    stopObserver();
observer_err:
    delete mDrm;
    mDrm = 0;
drm_err:
    return ret;
}

bool IntelHWComposer::prepareDisplays(size_t numDisplays,
                                      hwc_display_contents_1_t** displays)
{
    android::Mutex::Autolock _l(mLock);

    for (size_t disp = 0; disp < numDisplays; disp++) {
         hwc_display_contents_1_t *list = displays[disp];
         mDisplayDevice[disp]->prepare(list);

         if (disp && !list)
             signalHpdCompletion();
    }

    return true;
}

bool IntelHWComposer::commitDisplays(size_t numDisplays,
                                     hwc_display_contents_1_t** displays)
{
    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    android::Mutex::Autolock _l(mLock);

    IMG_hwc_layer_t imgHWCLayerList[INTEL_DISPLAY_PLANE_NUM];
    int numLayers = 0;

    for (size_t disp = 0; disp < numDisplays; disp++) {
         hwc_display_contents_1_t *list = displays[disp];

         if (list)
             mDisplayDevice[disp]->commit(list, imgHWCLayerList, numLayers);
    }

    // commit plane contexts
    if (mFBDev && numLayers) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: commits %d buffers\n", __func__, numLayers);
        int err = mFBDev->Post2(&mFBDev->base, imgHWCLayerList, numLayers);
        if (err) {
            ALOGE("%s: Post2 failed with errno %d\n", __func__, err);
            return false;
        }
    }

    return true;
}

bool IntelHWComposer::blankDisplay(int disp, int blank)
{
    bool ret=true;

    if ((disp<DISPLAY_NUM) && mDisplayDevice[disp])
        ret = mDisplayDevice[disp]->blank(blank);

    return ret;
}

bool IntelHWComposer::getDisplayConfigs(int disp, uint32_t* configs,
                                        size_t* numConfigs)
{
    if (disp >= DISPLAY_NUM) {
        ALOGW("%s: invalid disp num %d\n", __func__, disp);
        return false;
    }

    if (!mDisplayDevice[disp]->getDisplayConfig(configs, numConfigs)) {
        return false;
    }

    return true;
}

bool IntelHWComposer::getDisplayAttributes(int disp, uint32_t config,
            const uint32_t* attributes, int32_t* values)
{
    if (disp >= DISPLAY_NUM) {
        ALOGW("%s: invalid disp num %d\n", __func__, disp);
        return false;
    }

    if (!mDisplayDevice[disp]->getDisplayAttributes(config, attributes, values)) {
        return false;
    }

    return true;
}

bool IntelHWComposer::compositionComplete(int disp)
{
    if (mFBDev) {
        mFBDev->base.compositionComplete(&mFBDev->base);
    }
    return true;
}
