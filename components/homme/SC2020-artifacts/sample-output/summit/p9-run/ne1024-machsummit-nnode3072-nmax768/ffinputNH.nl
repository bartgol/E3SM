&ctl_nl
NThreads=-1
partmethod    = 4
topology      = "cube"
test_case     = "jw_baroclinic"
u_perturb = 1
rotate_grid = 0
ne=1024
qsize = 10
nmax=768
statefreq=99999999
disable_diagnostics = .true.
restartfreq   = 999999999
restartfile   = "./R0001"
runtype       = 0
mesh_file='/dev/null'
tstep=10      ! ne30: 300  ne120: 75
rsplit=2       ! ne30: 3   ne120:  2
qsplit = 1
tstep_type = 10
integration   = "explicit"
nu    =2.5e10 
nu_div=2.5e10
nu_p  =2.5e10
nu_q  =2.5e10
nu_s  =2.5e10
nu_top = 0
se_ftype     = 0
limiter_option = 9
vert_remap_q_alg = 1
hypervis_scaling=0
hypervis_order = 2
hypervis_subcycle=1    ! ne30: 3  ne120: 4
theta_hydrostatic_mode=.false.
theta_advect_form=1
hypervis_subcycle_tom  = 0
/

&vert_nl
vform         = "ccm"
vfile_mid = './sabm-128.ascii'
vfile_int = './sabi-128.ascii'
/

&prof_inparm
profile_outpe_num = 100
profile_single_file		= .true.
/

&analysis_nl
! disabled
 output_timeunits=1,1
 output_frequency=0,0
 output_start_time=0,0
 output_end_time=30000,30000
 output_varnames1='ps','zeta','T','geo'
 output_varnames2='Q','Q2','Q3','Q4','Q5'
 io_stride=8
 output_type = 'netcdf' 
/

