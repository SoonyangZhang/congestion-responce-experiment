# congestion-responce-experiment
A delay constraint BBR algorithm optimized for real time video transmission.  
And the wscript under ns3.26, change c++11 flag to c++14  
```
    # Enable C++-11 support
    env.append_value('CXXFLAGS', '-std=c++14')
```
Or else, the libquic will generating some annoying error:‘decay_t’ is not a member of ‘std’.  

As for the congestion control improvement code under the file delay-bbr, it depends on some third party external library.  
And the original repository is in https://github.com/SoonyangZhang/mp-video-simulation/tree/master/ns3/mp-video/model/congestion.
