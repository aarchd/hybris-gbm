project(
	'hybris_gbm',
	'c',
	version: '0.1',
	default_options: [
		'warning_level=3',
		'werror=true',
	],
)

shared_library(
	'hybris_gbm',
	files('main.c'),
	include_directories : ['.'],
	dependencies : [
		dependency('android-headers', required: true),
		dependency('libgralloc', required: true),
	],
	install : true,
	install_dir: join_paths(get_option('libdir'), 'gbm'),
	name_prefix: '',
)
