package require astro 2.0

proc ASSIGN {channel freq pol} {
  global xant_fd
  set msg "assign $channel $freq $pol"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "assign" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc BEEP {} {
  global xant_fd
  set msg "BEEP"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "beep" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc CLR {type coord} {
  global xant_fd
  set msg "clr $type $coord"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "clr" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc DELTA_UT {ms} {
  global xant_fd
  set msg "delta_ut $ms"
  puts $msg:
  set rsp [net_ask $xant_fd $msg]
  while { [string first "delta_ut" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc FEEDPOS {channel} {
  global xant_fd
  set msg "feedpos $channel"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "feedpos" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc LOAD_MODEL {name} {
  global xant_fd
  set msg "load_model $name"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "load_model" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc MINICAL {args} {
  global xant_fd
  set msg "minical $args"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "minical" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc AZEL {args} {
  global xant_fd
  set msg "azel $args"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "azel" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc POFFSET {args} {
  global xant_fd
  set msg "poffset $args"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "poffset" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc POFFSETS {coord value1 value2} {
  global xant_fd
  set msg "poffsets $coord $value1 $value2"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "poffsets" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc REFCHAN {args} {
  global xant_fd
  set msg "refchan $args"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "refchan" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc ROFFSET {coord value} {
  global xant_fd
  set msg "roffset $coord $value"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "roffset" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc SCAN {args} {
  global xant_fd
  set msg "scan $args"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "scan" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc SET_RAD_INT {value} {
  global xant_fd
  set msg "set_rad_int $value"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "set_rad_int" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc SOURCE {name args} {
  global xant_fd
  set msg "source $name $args"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "source" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc STARTUP {} {
  global xant_fd
  set msg "startup"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "startup" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc STOP {} {
  global xant_fd
  set msg "stop"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "stop" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc STOW {} {
  global xant_fd
  set msg "stow"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "stow" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc TLOG {args} {
  global xant_fd
  set msg "tlog $args"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "tlog" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc TRACK {} {
  global xant_fd
  set msg "track"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "track" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc TOGGLE_DISK {args} {
  global xant_fd
  set msg "toggle_disk $args"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "toggle_disk" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc WAIT {sec} {
  global xant_fd
  set msg "wait $sec"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "wait" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc WAIT_FOR_FEED {} {
  global xant_fd
  set msg "wait_for_feed"
  puts $msg
  set rsp [net_ask $xant_fd $msg]
  while { [string first "wait_for_feed" $rsp] != 0} {
    after 1000
    set rsp [lindex [net_recv $xant_fd] 0]
  }
  puts $rsp
}

proc WAIT_FOR_ON_POINT {args} {
  global xant_fd
  set msg "wait_for_on_point $args"
  puts $msg
  net_ask $xant_fd $msg
  set rsp [net_ask $xant_fd onpoint?]
  while { [string compare "onpoint: Tracking" $rsp] != 0 } {
    after 1000
    set rsp [net_ask $xant_fd onpoint?]
  }
  puts $rsp
}

proc no_leading_zeros { str } {
  set new [string trimleft $str 0]
  if { [string length $new] == 0 } { set new 0 }
  return $new
}

proc decimal_day { doy time } {
  set timestr [split $time :]
  set h [no_leading_zeros [lindex $timestr 0] ]
  set m [no_leading_zeros [lindex $timestr 1] ]
  set s [no_leading_zeros [lindex $timestr 2] ]
  set utd [expr $doy + ($h + ($m + $s/60.0)/60.0)/24]
  return $utd
}

proc jd_now {} {
  set datestr [exec date -u +%Y\ %j\ %T]
  set utn [decimal_day [no_leading_zeros [lindex $datestr 1] ] \
                       [lindex $datestr 2] ]
  set jd_now [julian_day [lindex $datestr 0] $utn]
  return $jd_now
}

proc WAIT_UNTIL {year doy UT} {
  set doy [no_leading_zeros $doy]
  set utd [decimal_day $doy $UT]
  set jd_req [julian_day $year $utd]
  puts "Waiting for $jd_req"

  while { [jd_now] < $jd_req } { after 1000 }
}

proc UNTIL_DO {year doy UT script} {
  set doy [no_leading_zeros $doy]
  set utd [decimal_day $doy $UT]
  set jd_req [julian_day $year $utd]
  puts "Waiting for $jd_req"

  while { [jd_now] < $jd_req } {
    eval $script
  }
}

proc DO_UNTIL {year doy UT script} {
  set doy [no_leading_zeros $doy]
  set utd [decimal_day $doy $UT]
  set jd_req [julian_day $year $utd]
  puts "Waiting for $jd_req"

  while { [jd_now] < $jd_req } {
    eval $script
  }
}
