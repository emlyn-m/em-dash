# em—dash

## config
Configuration is managed through two .env files - `.env` and `.env.config`. These must be loaded onto the kindle prior to running, and are expected to reside in the root directory. 

## cross-compiling

## tips+tricks

### disabling screensaver
using the revshell scriptlet, execute
```
lipc-set-prop com.lab126.powerd preventScreenSaver 1
```
*based on /usr/bin/ds.sh - seems like ~ds is broken as of 5.16.2.1.1 and possibly earlier*
