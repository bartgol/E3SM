&ctl_nl
theta_hydrostatic_mode = .false.
dcmip4_moist  = 0
dcmip4_X      = 1000.0
vert_num_threads = 1
NThreads=1
partmethod    = 4
topology      = "cube"
test_case     = "dcmip2012_test4"
u_perturb = 1
rotate_grid = 0
ne=11
qsize = 0
nmax = 12960
statefreq=270
restartfreq =   12960
restartfile   = "./restart/R000012960"
runtype       = 0
mesh_file='/dev/null'
tstep=.2
rsplit=3
qsplit = 1
tstep_type = 5
integration   = "explicit"
nu=2e7
nu_p=2e7
nu_q=2e7
nu_s=2e7
nu_top = 0
se_ftype     = 0
limiter_option = 8
vert_remap_q_alg = 0
hypervis_scaling=0
hypervis_order = 2
/
&vert_nl
vform         = "ccm"
vfile_mid = '../vcoord/camm-30.ascii'
vfile_int = '../vcoord/cami-30.ascii'
/

&prof_inparm
profile_outpe_num = 100
profile_single_file             = .true.
/

&analysis_nl
! to compare with EUL ref solution:
! interp_nlat = 512
! interp_nlon = 1024

 output_timeunits=0              ! 1- days, 2 hours, 0 - tsteps
 output_frequency=4320
 output_start_time=10
 output_end_time=12960
 output_varnames1='ps','zeta','u','v','T'
 num_io_procs      = 16
 output_type = 'netcdf'
 output_prefix = 'hydro-X1000-erun1-'
/





