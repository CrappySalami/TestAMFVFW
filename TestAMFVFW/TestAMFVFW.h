/******************************************************************************
Copyright (C) 2015 by jackun <jack.un@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once
#ifndef TESTAMFVFW_H
#define TESTAMFVFW_H

#include "BufferCopyManager.h"
#include "DeviceDX11.h"
#include <d3dcompiler.h>
#include "DeviceOCL.h"
#include "Log.h"
#include "Colorspace.h"
#include "Conversion.h"

/* Registry */
#define OVE_REG_KEY    HKEY_CURRENT_USER
#define OVE_REG_PARENT L"Software"
#define OVE_REG_CHILD  L"TestAMFVFW"
#define OVE_REG_CLASS  L"config"

extern HMODULE hmoduleVFW;
extern CRITICAL_SECTION lockCS;

#define MIN(a, b) (((a)<(b)) ? (a) : (b))
#define MAX(a, b) (((a)>(b)) ? (a) : (b))
#define CLIP(v, a, b) (((v)<(a)) ? (a) : ((v)>(b)) ? (b) : (v))

/* Settings */
#define S_LOG         "Log"
#define S_CABAC       "CABAC"
#define S_RCM         "RateControlMethod"
#define S_PRESET      "Preset"
#define S_PROFILE     "Profile"
#define S_LEVEL       "Level"
#define S_IDR         "IDRPeriod"
#define S_GOP         "GOPSize"
#define S_BITRATE     "Bitrate"
#define S_QPI         "QPI"
#define S_QPP         "QPP"
#define S_QPB         "QPB"
#define S_QP_MIN      "QPMin"
#define S_QP_MAX      "QPMax"
#define S_QPBD        "QPBDelta"
#define S_COLORPROF   "ColorProfile" //AMF_VIDEO_CONVERTER_COLOR_PROFILE_ENUM
#define S_DEVICEIDX   "DeviceIndex"
#define S_FPS_NUM     "FPSnum"
#define S_FPS_DEN     "FPSden"
#define S_FPS_ENABLED "FPSEnabled"
#define S_DISABLE_OCL "DisableOpenCL"
#define S_CONVTYPE    "ConverterType"

#define S_INSTALL     "InstallPath"

struct VfwState
{
	uint8_t log : 1;
	uint8_t cabac : 1;
	uint8_t rcm : 2;
	uint8_t preset : 2;
	uint8_t colorprof : 2;

	uint8_t fps_enabled : 1;
	uint8_t disable_ocl : 1;
	uint8_t convtype : 6;

	uint8_t profile;
	uint8_t level;
	uint8_t idr;
	uint8_t gop;
	uint32_t bitrate;
	uint8_t qpi;
	uint8_t qp_min;
	uint8_t qp_max;
	uint8_t device;
	uint8_t fps_num;
	uint8_t fps_den;
};

enum RCM {
	RCM_CQP = 0,
	RCM_CBR,
	RCM_PCVBR,
	RCM_LCVBR,
};

amf_pts AMF_STD_CALL amf_high_precision_clock();
void AMF_STD_CALL amf_increase_timer_precision();

template <class T> void SafeRelease(T **ptr)
{
	if (ptr && *ptr)
	{
		(*ptr)->Release();
		(*ptr) = nullptr;
	}
}

class Submitter
{
public:
	virtual DWORD Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts) = 0;
	virtual ~Submitter() {}
	virtual bool Init()
	{
		return true;
	}
};

class NV12Submitter;
class AMFConverterSubmitter;
class OpenCLSubmitter;
class DX11Submitter;
class DX11ComputeSubmitter;
class HostSubmitter;

class CodecInst {
	// Ehhhhh
	friend class Submitter;
	friend class NV12Submitter;
	friend class AMFConverterSubmitter;
	friend class OpenCLSubmitter;
	friend class DX11Submitter;
	friend class DX11ComputeSubmitter;
	friend class HostSubmitter;
private:
	Logger *mLog;
	int started;	//if the codec has been properly initalized yet
	HMODULE hModRuntime;
	AMFInit_Fn AMFInit = nullptr;
	AMFQueryVersion_Fn AMFQueryVersion = nullptr;
	amf::AMFFactory *mAMFFactory = nullptr;
	amf::AMFComputePtr mCompute;
	amf::AMFComputeDevicePtr mComputeDev;

	DeviceOCL mDeviceCL;
	amf::AMFContextPtr   mContext;
	amf::AMFComponentPtr mEncoder;
	amf::AMFComponentPtr mConverter;
	amf::AMF_MEMORY_TYPE mNativeMemType;
	amf::AMF_SURFACE_FORMAT mFmtIn;
	amf_pts mFrameDuration;

	unsigned int mLength;
	LONG mWidth;
	LONG mHeight;
	unsigned int mFormat;	//input format for compressing, output format for decompression. Also the bitdepth.
	unsigned int mCompression;
	bool mHasIDR;
	UINT32 mIDRPeriod;
	DWORD  mCompressedSize;
	bool mCLConv;
	std::unique_ptr<Submitter> mSubmitter;

	/* ICM_COMPRESS_FRAMES_INFO params */
	int frame_total;
	UINT32 fps_num;
	UINT32 fps_den;

	bool BindDLLs();
	void PrintProps(amf::AMFPropertyStorage *props);
	void SaveState(VfwState &state);
	void LoadState(const VfwState &state);

	//Argh, no ref counting so just dump it here
	HINSTANCE hD3DCompiler = nullptr;
	decltype(&D3DCompile) pfn_D3DCompile = nullptr;

	HRESULT WINAPI
		D3DCompile(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
			_In_ SIZE_T SrcDataSize,
			_In_opt_ LPCSTR pSourceName,
			_In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines,
			_In_opt_ ID3DInclude* pInclude,
			_In_opt_ LPCSTR pEntrypoint,
			_In_ LPCSTR pTarget,
			_In_ UINT Flags1,
			_In_ UINT Flags2,
			_Out_ ID3DBlob** ppCode,
			_Out_opt_ ID3DBlob** ppErrorMsgs)
	{
		if (pfn_D3DCompile)
			return pfn_D3DCompile(pSrcData,
				SrcDataSize,
				pSourceName,
				pDefines,
				pInclude,
				pEntrypoint,
				pTarget,
				Flags1,
				Flags2,
				ppCode,
				ppErrorMsgs);

		return E_INVALIDARG;
	}

public:
	DeviceDX11 mDeviceDX11;
	std::map<std::string, int32_t> mConfigTable;
	bool mDialogUpdated;

	bool SaveRegistry();
	bool ReadRegistry();
	void InitSettings();

	void LogMsg(bool msgBox, const wchar_t *psz_fmt, ...);

	CodecInst();
	~CodecInst();

	DWORD GetState(LPVOID pv, DWORD dwSize);
	DWORD SetState(LPVOID pv, DWORD dwSize);
	DWORD Configure(HWND hwnd);
	DWORD GetInfo(ICINFO* icinfo, DWORD dwSize);

	DWORD CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
	DWORD CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
	DWORD CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
	DWORD CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
	DWORD Compress(ICCOMPRESS* icinfo, DWORD dwSize);
	DWORD CompressEnd();
	DWORD CompressFramesInfo(ICCOMPRESSFRAMES *);

	DWORD DecompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
	DWORD DecompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
	DWORD DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
	DWORD Decompress(ICDECOMPRESS* icinfo, DWORD dwSize);
	DWORD DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
	DWORD DecompressEnd();

	BOOL QueryConfigure();
};

class NV12Submitter : public Submitter
{
public:
	CodecInst *mInstance;
	NV12Submitter(CodecInst *instance) : mInstance(instance)
	{}

	DWORD Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts);
};

class AMFConverterSubmitter : public Submitter
{
public:
	CodecInst *mInstance;
	ID3D11Texture2D *mTexStaging;
	AMFConverterSubmitter(CodecInst *instance) : mInstance(instance)
		, mTexStaging(nullptr)
	{}
	~AMFConverterSubmitter()
	{
		SafeRelease(&mTexStaging);
	}

	DWORD Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts);
};

class OpenCLSubmitter : public Submitter
{
public:
	CodecInst *mInstance;
	OpenCLSubmitter(CodecInst *instance) : mInstance(instance)
	{}

	DWORD Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts);
};

class DX11Submitter : public Submitter
{
public:
	CodecInst *mInstance;
	amf::AMFSurfacePtr surface;
	ID3D11Texture2D *mTexStaging = nullptr;
	BufferCopyManager	mBufferCopyManager;
	DX11Submitter(CodecInst *instance) : mInstance(instance)
	{
	}
	~DX11Submitter()
	{
		if (mTexStaging)
		{
			mTexStaging->Release();
			mTexStaging = nullptr;
		}
	}

	bool Init();
	DWORD Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts);
};

// Can only do 32bit RGB
class DX11ComputeSubmitter : public Submitter
{
	struct ConstBuffer
	{
		int inPitch;
		int colorspace;
		//IDK, 16byte align
		int pad1;
		int pad2;
	};

public:
	CodecInst*			mInstance;
	amf::AMFSurfacePtr	mSurface = nullptr;
	ID3D11Texture2D*	mTexStaging = nullptr;

	ID3D11Buffer*				mSrcBuffer = nullptr;
	ID3D11ShaderResourceView*	mSrcBufferView = nullptr;
	ID3D11DeviceContext*		mImmediateContext = nullptr;
	ID3D11UnorderedAccessView	*mUavNV12[2] = { nullptr, nullptr };
	ID3D11ComputeShader*		mComputeShader = nullptr;
	ID3D11Buffer* mConstantBuf = nullptr;
	BufferCopyManager mBufferCopyManager;

	DX11ComputeSubmitter(CodecInst *instance) : mInstance(instance)
	{
	}

	~DX11ComputeSubmitter()
	{
		if (!mSurface.GetPtr())
			SafeRelease(&mTexStaging);
		SafeRelease(&mUavNV12[0]);
		SafeRelease(&mUavNV12[1]);
		SafeRelease(&mSrcBuffer);
		SafeRelease(&mSrcBufferView);
		SafeRelease(&mComputeShader);
		SafeRelease(&mImmediateContext);
	}

	bool Init();
	bool loadComputeShader();
	DWORD Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts);
};

class HostSubmitter : public Submitter
{
public:
	CodecInst*			mInstance;
	BufferCopyManager	mBufferCopyManager;

	HostSubmitter(CodecInst *instance) : mInstance(instance)
	{
	}

	bool Init()
	{
		mBufferCopyManager.Start(3);
		return true;
	}

	DWORD Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts);
};

CodecInst* Open(ICOPEN* icinfo);
DWORD Close(CodecInst* pinst);
#endif