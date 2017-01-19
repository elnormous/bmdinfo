#include <iostream>
#include <cinttypes>
#include <thread>

#include "DeckLinkAPIVersion.h"
#include "DeckLinkAPI.h"
#include "DeckLinkAPIDispatch.cpp"

class QueryDelegate : public IDeckLinkInputCallback
{
public:
    QueryDelegate(IDeckLinkDisplayMode* mode);
    
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
    
    IDeckLinkDisplayMode* GetDisplayMode() const { return displayMode; }
    bool isDone() const { return done; }
                   
    private:
    	IDeckLinkDisplayMode* currentDisplayMode;
        IDeckLinkDisplayMode* displayMode = nullptr;
        ULONG refCount;
        bool done;
};

QueryDelegate::QueryDelegate(IDeckLinkDisplayMode* mode): currentDisplayMode(mode), refCount(0), done(false)
{
}

ULONG QueryDelegate::AddRef(void)
{
    refCount++;
    return refCount;
}

ULONG QueryDelegate::Release(void)
{
    refCount--;

    if (!refCount)
    {
        delete this;
        return 0;
    }

    return refCount;
}

HRESULT
QueryDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame,
                                      IDeckLinkAudioInputPacket* audioFframe)
{
    if (videoFrame->GetFlags() & bmdFrameHasNoInputSource)
    {
        return S_OK;
    }
    else
    {
    	displayMode = currentDisplayMode;
    	displayMode->AddRef();
        done = true;
    }
    
    return S_OK;
}

HRESULT
QueryDelegate::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents ev,
                                       IDeckLinkDisplayMode* mode,
                                       BMDDetectedVideoInputFormatFlags)
{
    displayMode = mode;
    displayMode->AddRef();
    done = true;
    return S_OK;
}

class Instance
{
public:
	Instance(IDeckLink* dl):
		deckLink(dl)
	{
	}

	~Instance()
	{
		if (displayModeIterator)
		{
	        displayModeIterator->Release();
	    }

	    if (input)
	    {
	        input->Release();
	    }

	    if (deckLink)
	    {
	        deckLink->Release();
	    }
	}

	bool detect(int videoConnection, int audioConnection)
	{
		HRESULT ret;

		IDeckLinkAttributes* deckLinkAttributes;
	    ret = deckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes);
	    
	    if (ret != S_OK)
	        return false;

	    bool formatDetectionSupported;
	    ret = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &formatDetectionSupported);
	    
	    if (ret != S_OK)
	        return false;
	    
	    if (!formatDetectionSupported)
	        return false;
	    
	    ret = deckLink->QueryInterface(IID_IDeckLinkInput, (void**)&input);
	    if (ret != S_OK)
	        return false;

	    ret = deckLink->QueryInterface(IID_IDeckLinkConfiguration, (void**)&configuration);
	    if (ret != S_OK)
	        return false;

	    switch (audioConnection)
	    {
	    case 1:
	        ret = configuration->SetInt(bmdDeckLinkConfigAudioInputConnection, bmdAudioConnectionAnalog);
	        break;
	    case 2:
	        ret = configuration->SetInt(bmdDeckLinkConfigAudioInputConnection, bmdAudioConnectionEmbedded);
	        break;
	    default:
	        // do not change it
	        break;
	    }

	    if (ret != S_OK)
	        return false;

	    switch (videoConnection)
	    {
	    case 1:
	        ret = configuration->SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionComposite);
	        break;
	    case 2:
	        ret = configuration->SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionComponent);
	        break;
	    case 3:
	        ret = configuration->SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionHDMI);
	        break;
	    case 4:
	        ret = configuration->SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionSDI);
	        break;
	    default:
	        // do not change it
	        break;
	    }

	    if (ret != S_OK)
	        return false;

	    ret = input->GetDisplayModeIterator(&displayModeIterator);

	    if (ret != S_OK)
	        return false;

	    while (true)
	    {
	        if (displayModeIterator->Next(&displayMode) != S_OK)
	        {
	            displayMode->Release();
	            displayMode = nullptr;
	        }
	        else
	        {
	            break;
	        }
	    }
	    
	    if (!displayMode)
	        return false;

	    QueryDelegate* delegate = new QueryDelegate(displayMode);

	    if (!delegate)
	        return false;

	    input->SetCallback(delegate);

	    ret = input->EnableVideoInput(displayMode->GetDisplayMode(),
	                               	  bmdFormat8BitYUV,
	                                  bmdVideoInputEnableFormatDetection);

	    ret = input->StartStreams();
	    
	    if (ret != S_OK)
	        return false;

	    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

	    while (!delegate->isDone())
	    {
	    	if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(2))
	    	{
	    		return false;
	    	}

        	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	    }
	    
	    ret = input->StopStreams();
	    
	    if (ret != S_OK)
	        return false;

	    if (!delegate->GetDisplayMode())
	    {
	    	return false;
	    }

	    std::cout << "video mode: ";

	    switch (delegate->GetDisplayMode()->GetDisplayMode())
	    {
	        case bmdModeNTSC: std::cout << 0 << ", bmdModeNTSC"; break;
	        case bmdModeNTSC2398: std::cout << 1 << ", bmdModeNTSC2398"; break;
	        case bmdModePAL: std::cout << 2 << ", bmdModePAL"; break;
	        case bmdModeNTSCp: std::cout << 14 << ", bmdModeNTSCp"; break;
	        case bmdModePALp: std::cout << 15 << ", bmdModePALp"; break;

	        case bmdModeHD1080p2398: std::cout << 3 << ", bmdModeHD1080p2398"; break;
	        case bmdModeHD1080p24: std::cout << 4 << ", bmdModeHD1080p24"; break;
	        case bmdModeHD1080p25: std::cout << 5 << ", bmdModeHD1080p25"; break;
	        case bmdModeHD1080p2997: std::cout << 6 << ", bmdModeHD1080p2997"; break;
	        case bmdModeHD1080p30: std::cout << 7 << ", bmdModeHD1080p30"; break;
	        case bmdModeHD1080i50: std::cout << 8 << ", bmdModeHD1080i50"; break;
	        case bmdModeHD1080i5994: std::cout << 9 << ", bmdModeHD1080i5994"; break;
	        case bmdModeHD1080i6000: std::cout << 10 << ", bmdModeHD1080i6000"; break;
	        case bmdModeHD1080p50: std::cout << 16 << ", bmdModeHD1080p50"; break;
	        case bmdModeHD1080p5994: std::cout << 17 << ", bmdModeHD1080p5994"; break;
	        case bmdModeHD1080p6000: std::cout << 18 << ", bmdModeHD1080p6000"; break;

	        case bmdModeHD720p50: std::cout << 11 << ", bmdModeHD720p50"; break;
	        case bmdModeHD720p5994: std::cout << 12 << ", bmdModeHD720p5994"; break;
	        case bmdModeHD720p60: std::cout << 13 << ", bmdModeHD720p60"; break;

	        case bmdMode2k2398: std::cout << 19 << ", bmdMode2k2398"; break;
	        case bmdMode2k24: std::cout << 20 << ", bmdMode2k24"; break;
	        case bmdMode2k25: std::cout << 21 << ", bmdMode2k25"; break;

	        case bmdMode2kDCI2398: std::cout << 22 << ", bmdMode2kDCI2398"; break;
	        case bmdMode2kDCI24: std::cout << 23 << ", bmdMode2kDCI24"; break;
	        case bmdMode2kDCI25: std::cout << 24 << ", bmdMode2kDCI25"; break;

	        case bmdMode4K2160p2398: std::cout << 25 << ", bmdMode4K2160p2398"; break;
	        case bmdMode4K2160p24: std::cout << 26 << ", bmdMode4K2160p24"; break;
	        case bmdMode4K2160p25: std::cout << 27 << ", bmdMode4K2160p25"; break;
	        case bmdMode4K2160p2997: std::cout << 28 << ", bmdMode4K2160p2997"; break;
	        case bmdMode4K2160p30: std::cout << 29 << ", bmdMode4K2160p30"; break;

	#if BLACKMAGIC_DECKLINK_API_VERSION >= 0x0a030100
	        case bmdMode4K2160p50: std::cout << 30 << ", bmdMode4K2160p50"; break;
	        case bmdMode4K2160p5994: std::cout << 31 << ", bmdMode4K2160p5994"; break;
	        case bmdMode4K2160p60: std::cout << 32 << ", bmdMode4K2160p60"; break;
	#endif

	        case bmdMode4kDCI2398: std::cout << 33 << ", bmdMode4kDCI2398"; break;
	        case bmdMode4kDCI24: std::cout << 34 << ", bmdMode4kDCI24"; break;
	        case bmdMode4kDCI25: std::cout << 35 << ", bmdMode4kDCI25"; break;
	    }

	    int64_t num, den;
    	delegate->GetDisplayMode()->GetFrameRate(&num, &den);

	    std::cout << ", resolution: " << delegate->GetDisplayMode()->GetWidth() << "x" << delegate->GetDisplayMode()->GetHeight() << ", framerate: " << static_cast<float>(num) / den << ", field dominance: ";

	    switch (delegate->GetDisplayMode()->GetFieldDominance())
	    {
	    case bmdUnknownFieldDominance:
	        std::cout << "unknown";
	        break;
	    case bmdLowerFieldFirst:
	        std::cout << "lower field first";
	        break;
	    case bmdUpperFieldFirst:
	        std::cout << "upper field first";
	        break;
	    case bmdProgressiveFrame:
	        std::cout << "progressive frame";
	        break;
	    case bmdProgressiveSegmentedFrame:
	        std::cout << "progressive segmented frame";
	        break;
	    default:
	    	std::cout << "could not detect";
	    	break;
	    }

	    std::cout << std::endl;

	    delegate->GetDisplayMode()->Release();

	    return true;
	}

private:
	IDeckLink* deckLink;
    IDeckLinkInput* input = nullptr;
    IDeckLinkDisplayModeIterator* displayModeIterator = nullptr;
    IDeckLinkDisplayMode* displayMode = nullptr;
    IDeckLinkConfiguration* configuration = nullptr;
};

int main()
{
    HRESULT ret;
    uint32_t i = 0;

    IDeckLinkIterator* deckLinkIterator = CreateDeckLinkIteratorInstance();

    if (!deckLinkIterator)
    {
        return 1;
    }

    while (true)
    {
    	IDeckLink* deckLink = nullptr;
        ret = deckLinkIterator->Next(&deckLink);

        if (ret != S_OK)
        {
    		deckLinkIterator->Release();
        	return 0;
        }

        Instance instance(deckLink);
        
        std::cout << "Instance: " << i << ", ";
		if (!instance.detect(4, 2))
		{
			std::cout << "failed to detect video mode" << std::endl;
		}

	    if (deckLink)
	        deckLink->Release();

	    ++i;
    }

    if (deckLinkIterator)
    {
    	deckLinkIterator->Release();
    }

    return 0;
}
