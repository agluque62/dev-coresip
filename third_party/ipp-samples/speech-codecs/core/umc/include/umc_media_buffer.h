/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2007 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __UMC_MEDIA_BUFFER_H__
#define __UMC_MEDIA_BUFFER_H__

#include "umc_structures.h"
#include "umc_dynamic_cast.h"
#include "umc_media_receiver.h"
#include "umc_memory_allocator.h"

namespace UMC
{

class MediaBufferParams : public MediaReceiverParams
{
    DYNAMIC_CAST_DECL(MediaBufferParams, MediaReceiverParams)

public:
    // Default constructor
    MediaBufferParams(void);

    size_t m_prefInputBufferSize;                               // (size_t) preferable size of input potion(s)
    Ipp32u m_numberOfFrames;                                    // (Ipp32u) minimum number of data potion in buffer
    size_t m_prefOutputBufferSize;                              // (size_t) preferable size of output potion(s)

    MemoryAllocator *m_pMemoryAllocator;                        // (MemoryAllocator *) pointer to memory allocator object
};

class MediaBuffer : public MediaReceiver
{
    DYNAMIC_CAST_DECL(MediaBuffer, MediaReceiver)

public:

    // Constructor
    MediaBuffer(void);
    // Destructor
    virtual
    ~MediaBuffer(void);

    // Lock output buffer
    virtual
    Status LockOutputBuffer(MediaData *out) = 0;
    // Unlock output buffer
    virtual
    Status UnLockOutputBuffer(MediaData *out) = 0;

    // Initialize the buffer
    virtual
    Status Init(MediaReceiverParams *pInit);

    // Release object
    virtual
    Status Close(void);

protected:

    MemoryAllocator *m_pMemoryAllocator;                        // (MemoryAllocator *) pointer to memory allocator object
    MemoryAllocator *m_pAllocated;                              // (MemoryAllocator *) owned pointer to memory allocator object
};

} // end namespace UMC

#endif // __UMC_MEDIA_BUFFER_H__
