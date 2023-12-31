#ifndef SND_BASS_H
#define SND_BASS_H

namespace BASS
{
typedef DWORD (WINAPI *TGetVersion)();
typedef BOOL  (WINAPI *TGetInfo)(BASS_INFO*);
typedef int   (WINAPI *TErrorGetCode)();
typedef BOOL  (WINAPI *TInit)(int,DWORD,DWORD,HWND,GUID*);
typedef BOOL  (WINAPI *TFree)();
typedef BOOL  (WINAPI *TPause)();
typedef BOOL  (WINAPI *TStart)();
typedef BOOL  (WINAPI *TStop)();
typedef BOOL  (WINAPI *TSetConfig)(DWORD,DWORD);
typedef DWORD (WINAPI *TGetConfig)(DWORD);

typedef BOOL  (WINAPI *TMusicFree)(HMUSIC);
typedef HMUSIC(WINAPI *TMusicLoad)(BOOL,void*,QWORD,DWORD,DWORD,DWORD);
typedef BOOL  (WINAPI *TChannelPause)(DWORD);
typedef BOOL  (WINAPI *TChannelPlay)(DWORD,BOOL);
typedef BOOL (WINAPI *TChannelStop)(DWORD);

typedef QWORD (WINAPI *TChannelGetPosition)(DWORD handle, DWORD mode);
typedef BOOL  (WINAPI *TChannelSetPosition)(DWORD,QWORD,DWORD);
typedef DWORD (WINAPI *TChannelGetLevel)(DWORD);
typedef BOOL  (WINAPI *TChannelFlags)(DWORD,DWORD,DWORD);
typedef double (WINAPI *TChannelBytes2Seconds)(DWORD,QWORD);
typedef BOOL  (WINAPI *TChannelSetAttribute)(DWORD,DWORD,float);
typedef BOOL  (WINAPI *TChannelGetAttribute)(DWORD handle, DWORD attrib, float *value);
typedef DWORD (WINAPI *TChannelIsActive)(DWORD handle);

typedef HSTREAM (WINAPI *TStreamCreate)(DWORD,DWORD,DWORD,STREAMPROC,PVOID);
typedef HSTREAM (WINAPI *TStreamCreateFileUser)(DWORD,DWORD,const BASS_FILEPROCS*,void*);
typedef BOOL  (WINAPI *TStreamFree)(HSTREAM);

extern TGetVersion          GetVersion;
extern TInit                Init;
extern TFree                Free;
extern TPause               Pause;
extern TStart               Start;
extern TStop                Stop;
extern TGetConfig           GetConfig;
extern TSetConfig           SetConfig;
extern TGetInfo             GetInfo;

extern TMusicFree           MusicFree;
extern TMusicLoad           MusicLoad;
extern TChannelPause        ChannelPause;
extern TChannelPlay         ChannelPlay;
extern TChannelStop         ChannelStop;

extern TChannelGetPosition  ChannelGetPosition;
extern TChannelSetPosition  ChannelSetPosition;
extern TChannelSetAttribute ChannelSetAttribute;
extern TChannelGetAttribute ChannelGetAttribute;

extern TChannelGetLevel     ChannelGetLevel;
extern TErrorGetCode        ErrorGetCode;
extern TChannelFlags        ChannelFlags;
extern TChannelBytes2Seconds ChannelBytes2Seconds;

extern TChannelIsActive ChannelIsActive;

extern TStreamCreate        StreamCreate;
extern TStreamCreateFileUser StreamCreateFileUser;
extern TStreamFree          StreamFree;

extern HMODULE Bass;
extern bool Initialized;

void Load();
void Unload();
}

#endif
