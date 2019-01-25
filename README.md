# congestion-responce-experiment
experiment on the performance of bbr  
And the wscript under ns3.26, change c++11 flag to c++14  
```
    # Enable C++-11 support
    env.append_value('CXXFLAGS', '-std=c++14')
```
Or else, the libquic will generating some annoying error:‘decay_t’ is not a member of ‘std’.  
