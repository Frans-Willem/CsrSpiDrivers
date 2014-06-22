@ cdecl spifns_getvarlist(ptr ptr)
@ cdecl spifns_init()
@ cdecl spifns_getvar(ptr)
@ cdecl spifns_get_last_error(ptr ptr)
@ cdecl spifns_set_debug_callback(ptr)
@ cdecl spifns_get_version()
@ cdecl spifns_open_port(long)
@ cdecl spifns_close_port()
@ varargs spifns_debugout(ptr)
@ cdecl spifns_close()
@ cdecl spifns_chip_select(long)
@ cdecl spifns_command(str)
@ cdecl spifns_enumerate_ports(ptr ptr)
@ cdecl spifns_sequence_setvar_spishiftperiod(long)
@ cdecl spifns_sequence_setvar_spiport(long)
@ cdecl spifns_debugout_readwrite(long long long ptr)
@ cdecl spifns_sequence_write(long long ptr)
@ cdecl spifns_sequence_setvar_spimul(long)
@ cdecl spifns_sequence_setvar(str str)
@ cdecl spifns_sequence_read(long long ptr)
@ cdecl spifns_sequence(ptr long)
@ cdecl spifns_bluecore_xap_stopped()
@ cdecl spifns_clear_last_error()
@ stub spifns_count_streams
@ stub spifns_get_last_error32
@ stub spifns_stream_bluecore_xap_stopped
@ stub spifns_stream_chip_select
@ stub spifns_stream_close
@ stub spifns_stream_command
@ stub spifns_stream_get_device_id
@ stub spifns_stream_getvar
@ stub spifns_stream_init
@ stub spifns_stream_lock
@ stub spifns_stream_sequence
@ stub spifns_stream_set_debug_callback
@ stub spifns_stream_unlock
