#include <iostream>
#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include "DeckLinkAPIVersion.h"
#include "DeckLinkAPI.h"
#include "DeckLinkAPIDispatch.cpp"

class QueryDelegate : public IDeckLinkInputCallback
{
public:
    QueryDelegate(BMDDisplayMode display_mode);
    
    virtual HRESULT STDMETHODCALLTYPE
        QueryInterface(REFIID iid, LPVOID *ppv) { return E_NOINTERFACE; }
    virtual ULONG STDMETHODCALLTYPE
        AddRef(void);
    virtual ULONG STDMETHODCALLTYPE
        Release(void);
    
    virtual HRESULT STDMETHODCALLTYPE
        VideoInputFormatChanged(BMDVideoInputFormatChangedEvents,
                                IDeckLinkDisplayMode*,
                                BMDDetectedVideoInputFormatFlags);
    
    virtual HRESULT STDMETHODCALLTYPE
        VideoInputFrameArrived(IDeckLinkVideoInputFrame*,
                               IDeckLinkAudioInputPacket*);
    
    BMDDisplayMode GetDisplayMode() const { return display_mode; }
    bool isDone() const { return done; }
                   
    private:
        BMDDisplayMode display_mode;
        ULONG ref_count;
        bool done;
};

QueryDelegate::QueryDelegate(BMDDisplayMode display_mode): display_mode(display_mode), ref_count(0), done(false)
{
}

ULONG QueryDelegate::AddRef(void)
{
    ref_count++;
    return ref_count;
}

ULONG QueryDelegate::Release(void)
{
    ref_count--;

    if (!ref_count) {
        delete this;
        return 0;
    }

    return ref_count;
}

HRESULT
QueryDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame  *v_frame,
                                        IDeckLinkAudioInputPacket *a_frame)
{
    if (v_frame->GetFlags() & bmdFrameHasNoInputSource)
    {
        return S_OK;
    }
    else
    {
        done = true;
    }
    
    return S_OK;
}

HRESULT
QueryDelegate::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents ev,
                                         IDeckLinkDisplayMode *mode,
                                         BMDDetectedVideoInputFormatFlags)
{
    display_mode = mode->GetDisplayMode();
    done = true;
    return S_OK;
}

class Instance
{
public:
	Instance(IDeckLink* deckLink):
		dl(deckLink)
	{
	}

	~Instance()
	{
		if (dm_it)
		{
	        dm_it->Release();
	    }

	    if (in)
	    {
	        in->Release();
	    }

	    if (dl)
	    {
	        dl->Release();
	    }
	}

	bool detect(int video_connection, int audio_connection)
	{
		HRESULT ret;

		IDeckLinkAttributes* deckLinkAttributes;
	    ret = dl->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes);
	    
	    if (ret != S_OK)
	        return false;

	    bool formatDetectionSupported;
	    ret = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &formatDetectionSupported);
	    
	    if (ret != S_OK)
	        return false;
	    
	    if (!formatDetectionSupported)
	        return false;
	    
	    ret = dl->QueryInterface(IID_IDeckLinkInput, (void**)&in);
	    if (ret != S_OK)
	        return false;

	    ret = dl->QueryInterface(IID_IDeckLinkConfiguration, (void**)&conf);

	    switch (audio_connection)
	    {
	    case 1:
	        ret = conf->SetInt(bmdDeckLinkConfigAudioInputConnection, bmdAudioConnectionAnalog);
	        break;
	    case 2:
	        ret = conf->SetInt(bmdDeckLinkConfigAudioInputConnection, bmdAudioConnectionEmbedded);
	        break;
	    default:
	        // do not change it
	        break;
	    }

	    if (ret != S_OK)
	        return false;

	    switch (video_connection)
	    {
	    case 1:
	        ret = conf->SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionComposite);
	        break;
	    case 2:
	        ret = conf->SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionComponent);
	        break;
	    case 3:
	        ret = conf->SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionHDMI);
	        break;
	    case 4:
	        ret = conf->SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionSDI);
	        break;
	    default:
	        // do not change it
	        break;
	    }

	    if (ret != S_OK)
	        return false;

	    ret = in->GetDisplayModeIterator(&dm_it);

	    if (ret != S_OK)
	        return false;

	    while (true)
	    {
	        if (dm_it->Next(&dm) != S_OK)
	        {
	            dm->Release();
	            dm = nullptr;
	        }
	        else
	        {
	            break;
	        }
	    }
	    
	    if (!dm)
	        return false;

	    QueryDelegate* delegate = new QueryDelegate(dm->GetDisplayMode());

	    if (!delegate)
	        return false;

	    in->SetCallback(delegate);

	    ret = in->EnableVideoInput(dm->GetDisplayMode(),
	                               bmdFormat8BitYUV,
	                               bmdVideoInputEnableFormatDetection);

	    ret = in->StartStreams();
	    
	    if (ret != S_OK)
	        return false;
	    
	    while (!delegate->isDone())
	    {
	        usleep(20000);
	    }
	    
	    ret = in->StopStreams();
	    
	    if (ret != S_OK)
	        return false;
	    
	    std::cout << "Video mode: ";

	    switch (delegate->GetDisplayMode())
	    {
	        case bmdModeNTSC: std::cout << 0 << ", bmdModeNTSC" << std::endl; break;
	        case bmdModeNTSC2398: std::cout << 1 << ", bmdModeNTSC2398" << std::endl; break;
	        case bmdModePAL: std::cout << 2 << ", bmdModePAL" << std::endl; break;
	        case bmdModeNTSCp: std::cout << 14 << ", bmdModeNTSCp" << std::endl; break;
	        case bmdModePALp: std::cout << 15 << ", bmdModePALp" << std::endl; break;

	        case bmdModeHD1080p2398: std::cout << 3 << ", bmdModeHD1080p2398" << std::endl; break;
	        case bmdModeHD1080p24: std::cout << 4 << ", bmdModeHD1080p24" << std::endl; break;
	        case bmdModeHD1080p25: std::cout << 5 << ", bmdModeHD1080p25" << std::endl; break;
	        case bmdModeHD1080p2997: std::cout << 6 << ", bmdModeHD1080p2997" << std::endl; break;
	        case bmdModeHD1080p30: std::cout << 7 << ", bmdModeHD1080p30" << std::endl; break;
	        case bmdModeHD1080i50: std::cout << 8 << ", bmdModeHD1080i50" << std::endl; break;
	        case bmdModeHD1080i5994: std::cout << 9 << ", bmdModeHD1080i5994" << std::endl; break;
	        case bmdModeHD1080i6000: std::cout << 10 << ", bmdModeHD1080i6000" << std::endl; break;
	        case bmdModeHD1080p50: std::cout << 16 << ", bmdModeHD1080p50" << std::endl; break;
	        case bmdModeHD1080p5994: std::cout << 17 << ", bmdModeHD1080p5994" << std::endl; break;
	        case bmdModeHD1080p6000: std::cout << 18 << ", bmdModeHD1080p6000" << std::endl; break;

	        case bmdModeHD720p50: std::cout << 11 << ", bmdModeHD720p50" << std::endl; break;
	        case bmdModeHD720p5994: std::cout << 12 << ", bmdModeHD720p5994" << std::endl; break;
	        case bmdModeHD720p60: std::cout << 13 << ", bmdModeHD720p60" << std::endl; break;

	        case bmdMode2k2398: std::cout << 19 << ", bmdMode2k2398" << std::endl; break;
	        case bmdMode2k24: std::cout << 20 << ", bmdMode2k24" << std::endl; break;
	        case bmdMode2k25: std::cout << 21 << ", bmdMode2k25" << std::endl; break;

	        case bmdMode2kDCI2398: std::cout << 22 << ", bmdMode2kDCI2398" << std::endl; break;
	        case bmdMode2kDCI24: std::cout << 23 << ", bmdMode2kDCI24" << std::endl; break;
	        case bmdMode2kDCI25: std::cout << 24 << ", bmdMode2kDCI25" << std::endl; break;

	        case bmdMode4K2160p2398: std::cout << 25 << ", bmdMode4K2160p2398" << std::endl; break;
	        case bmdMode4K2160p24: std::cout << 26 << ", bmdMode4K2160p24" << std::endl; break;
	        case bmdMode4K2160p25: std::cout << 27 << ", bmdMode4K2160p25" << std::endl; break;
	        case bmdMode4K2160p2997: std::cout << 28 << ", bmdMode4K2160p2997" << std::endl; break;
	        case bmdMode4K2160p30: std::cout << 29 << ", bmdMode4K2160p30" << std::endl; break;

	#if BLACKMAGIC_DECKLINK_API_VERSION >= 0x0a030100
	        case bmdMode4K2160p50: std::cout << 30 << ", bmdMode4K2160p50" << std::endl; break;
	        case bmdMode4K2160p5994: std::cout << 31 << ", bmdMode4K2160p5994" << std::endl; break;
	        case bmdMode4K2160p60: std::cout << 32 << ", bmdMode4K2160p60" << std::endl; break;
	#endif

	        case bmdMode4kDCI2398: std::cout << 33 << ", bmdMode4kDCI2398" << std::endl; break;
	        case bmdMode4kDCI24: std::cout << 34 << ", bmdMode4kDCI24" << std::endl; break;
	        case bmdMode4kDCI25: std::cout << 35 << ", bmdMode4kDCI25" << std::endl; break;

	        default: std::cout << -1 << ", unknown" << std::endl; break;
	    }

	    return true;
	}

private:
	IDeckLink* dl;
    IDeckLinkInput*in = nullptr;
    IDeckLinkDisplayModeIterator* dm_it = nullptr;
    IDeckLinkDisplayMode* dm = nullptr;
    IDeckLinkConfiguration* conf = nullptr;
};

int main()
{
    HRESULT ret;
    uint32_t i = 0;

    IDeckLinkIterator* it = CreateDeckLinkIteratorInstance();

    if (!it)
    {
        return 1;
    }

    while (true)
    {
    	IDeckLink* dl = nullptr;
        ret = it->Next(&dl);

        if (ret != S_OK)
        {
    		it->Release();
        	return 0;
        }

        Instance instance(dl);
        
        std::cout << "instance: " << i << std::endl;
		if (!instance.detect(4, 2))
		{
			std::cout << "Failed to detect" << std::endl;
		}

	    if (dl)
	        dl->Release();

	    ++i;
    }

    return 0;
}
