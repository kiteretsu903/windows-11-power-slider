#pragma once
#include <windows.h>
#include <uianimation.h>
#include <cmath>

class NativeMotion {
public:
    ~NativeMotion() { shutdown(); }
    NativeMotion()=default;
    NativeMotion(const NativeMotion&)=delete;
    NativeMotion& operator=(const NativeMotion&)=delete;

    bool start(double from,double to,bool closing) noexcept {
        cancel();
        if(!initialize()) return false;
        IUIAnimationTransition2* transition=nullptr;
        HRESULT hr=manager_->CreateAnimationVariable(from,&variable_);
        // Fluent entrance (0,0,0,1) / exit (1,0,1,1), evaluated by Windows.
        if(SUCCEEDED(hr)) hr=library_->CreateCubicBezierLinearTransition(
            closing?.167:.250,to,closing?1.0:0.0,0.0,closing?1.0:0.0,1.0,&transition);
        if(SUCCEEDED(hr)) hr=manager_->CreateStoryboard(&story_);
        if(SUCCEEDED(hr)) hr=story_->AddTransition(variable_,transition);
        if(SUCCEEDED(hr)) hr=timer_->GetTime(&start_);
        UI_ANIMATION_SCHEDULING_RESULT scheduled=UI_ANIMATION_SCHEDULING_UNEXPECTED_FAILURE;
        if(SUCCEEDED(hr)) hr=story_->Schedule(start_,&scheduled);
        if(transition) transition->Release();
        if(FAILED(hr)||scheduled!=UI_ANIMATION_SCHEDULING_SUCCEEDED) {cancel();return false;}
        destination_=to;
        return true;
    }

    bool sample(int& value,bool& finished) noexcept {
        if(!story_) return false;
        double now=0,v=0;
        HRESULT hr=timer_->GetTime(&now);
        if(SUCCEEDED(hr)) hr=manager_->Update(now);
        if(SUCCEEDED(hr)) hr=variable_->GetValue(&v);
        UI_ANIMATION_STORYBOARD_STATUS status=UI_ANIMATION_STORYBOARD_BUILDING;
        if(SUCCEEDED(hr)) hr=story_->GetStatus(&status);
        if(FAILED(hr)) return false;
        // FINISHED is delivered only inside status-change callbacks. Polling
        // observes READY once a successfully scheduled storyboard has ended.
        finished=status==UI_ANIMATION_STORYBOARD_READY || now-start_>1.0;
        value=int(std::lround(finished?destination_:v));
        return true;
    }

    void cancel() noexcept {
        if(story_) {story_->Abandon();story_->Release();story_=nullptr;}
        if(variable_) {variable_->Release();variable_=nullptr;}
    }
    void shutdown() noexcept {
        cancel();
        if(manager_) {manager_->Shutdown();manager_->Release();manager_=nullptr;}
        if(library_) {library_->Release();library_=nullptr;}
        if(timer_) {timer_->Release();timer_=nullptr;}
    }
private:
    bool initialize() noexcept {
        if(manager_ && library_ && timer_) return true;
        shutdown();
        HRESULT hr=CoCreateInstance(__uuidof(UIAnimationManager2),nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IUIAnimationManager2),reinterpret_cast<void**>(&manager_));
        if(SUCCEEDED(hr)) hr=CoCreateInstance(__uuidof(UIAnimationTransitionLibrary2),nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IUIAnimationTransitionLibrary2),reinterpret_cast<void**>(&library_));
        if(SUCCEEDED(hr)) hr=CoCreateInstance(__uuidof(UIAnimationTimer),nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IUIAnimationTimer),reinterpret_cast<void**>(&timer_));
        if(FAILED(hr)) shutdown();
        return SUCCEEDED(hr);
    }
    IUIAnimationManager2* manager_{};
    IUIAnimationTransitionLibrary2* library_{};
    IUIAnimationTimer* timer_{};
    IUIAnimationVariable2* variable_{};
    IUIAnimationStoryboard2* story_{};
    double start_{},destination_{};
};
