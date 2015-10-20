# <a id="icinga2-api"></a> Icinga 2 API

## <a id="icinga2-api-introduction"></a> Introduction

The Icinga 2 API allows you to manage configuration objects
and resources in a simple, programmatic way using HTTP requests.

The endpoints are logically separated allowing you to easily
make calls to

* run [actions](9-icinga2-api.md#icinga2-api-actions) (reschedule checks, etc.)
* query, create, modify and delete [config objects](9-icinga2-api.md#icinga2-api-config-objects)
* [manage configuration packages](9-icinga2-api.md#icinga2-api-config-management)
* subscribe to [event streams](9-icinga2-api.md#icinga2-api-event-streams)

This chapter will start with a general overview followed by
detailed information about specific endpoints.

### <a id="icinga2-api-requests"></a> Requests

Any tool capable of making HTTP requests can communicate with
the API, for example [curl](http://curl.haxx.se).

Requests are only allowed to use the HTTPS protocol so that
traffic remains encrypted.

By default the Icinga 2 API listens on port `5665` sharing this
port with the cluster communication protocol. This can be changed
by setting the `bind_port` attribute in the [ApiListener](6-object-types.md#objecttype-apilistener)
configuration object in the `/etc/icinga2/features-available/api.conf`
file.

Supported request methods:

  Method	| Usage
  --------------|------------------------------------------------------
  GET		| Retrieve information about configuration objects. Any request using the GET method is read-only and does not affect any objects.
  POST		| Update attributes of a specified configuration object.
  PUT		| Create a new object. The PUT request must include all attributes required to create a new object.
  DELETE	| Remove an object created by the API. The DELETE method is idempotent and does not require any check if the object actually exists.

### <a id="icinga2-api-http-statuses"></a> HTTP Statuses

The API will return standard [HTTP statuses](https://www.ietf.org/rfc/rfc2616.txt)
including error codes.

When an error occurs, the response body will contain additional information
about the problem and its source.

A status in the range of 200 generally means that the request was succesful
and no error was encountered.

Return codes within the 400 range indicate that there was a problem with the
request. Either you did not authenticate correctly, you are missing the authorization
for your requested action, the requested object does not exist or the request
was malformed.

A status in the range of 500 generally means that there was a server-side problem
and Icinga 2 is unable to process your request currently.

Ask your Icinga 2 system administrator to check the `icinga2.log` file for further
troubleshooting.


### <a id="icinga2-api-responses"></a> Responses

Succesful requests will send back a response body containing a `results`
list. Depending on the number of affected objects in your request, the
results may contain one or more entries.

The [output](9-icinga2-api.md#icinga2-api-output) will be sent back as JSON object:


    {
        "results": [
            {
                "code": 200.0,
                "status": "Object was created."
            }
        ]
    }


### <a id="icinga2-api-authentication"></a> Authentication

There are two different ways for authenticating against the Icinga 2 API:

* username and password using HTTP basic auth
* X.509 certificate with client CN

In order to configure a new API user you'll need to add a new [ApiUser](6-object-types.md#objecttype-apiuser)
configuration object. In this example `root` will be the basic auth username
and the `password` attribute contains the basic auth password.

    vim /etc/icinga2/conf.d/api-users.conf

    object ApiUser "root" {
      password = icinga"
    }

Alternatively you can use X.509 client certificates by specifying the `client_cn`
the API should trust.

    vim /etc/icinga2/conf.d/api-users.conf

    object ApiUser "api-clientcn" {
      password = "CertificateCommonName"
    }

An `ApiUser` object can have both methods configured. Sensitive information
such as the password will not be exposed through the API itself.

New installations of Icinga 2 will automatically generate a new `ApiUser`
named `root` with a generated password in the `/etc/icinga2/conf.d/api-users.conf`
file.
You can manually invoke the cli command `icinga2 api setup` which will generate
a new local CA, self-signed certificate and a new API user configuration.

Once the API user is configured make sure to restart Icinga 2:

    # service icinga2 restart

Now pass the basic auth information to curl and send a GET request to the API:

    $ curl -u root:icinga -k -s 'https://localhost:5665/v1/status'

In case you will get an error message make sure to check the API user credentials.

### <a id="icinga2-api-permissions"></a> Permissions

By default an api user does not have any permissions to perform
actions on the [url endpoints](9-icinga2-api.md#icinga2-api-url-endpoints).

Permissions for api users must be specified in the `permissions` attribute
as array. The array items can be a list of permission strings with wildcard
matches.

Example for an api user with all permissions:

    permissions = [ "*" ]

A yet more sophisticated approach is to specify additional permissions
and their filters. The latter must be defined as [lamdba function](20-language-reference.md#nullary-lambdas)
returning a boolean expression.

The `permission` attribute contains the action and the specific capitalized
object type name. Instead of the type name it is also possible to use a wildcard
match.

The following example allows the api user to query all hosts and services with
the custom host attribute `os` matching the regular expression `^Linux`.

    permissions = [
      {
        permission = "objects/query/Host"
        filter = {{ regex("^Linux", host.vars.os)  }}
      },
      {
        permission = "objects/query/Service"
        filter = {{ regex("^Linux", host.vars.os)  }}
      },
    ]


Available permissions for specific url endpoints:

  Permissions				| Url Endpoint
  --------------------------------------|------------------------
  actions/;&lt;action;&gt;		| /v1/actions
  config/query				| /v1/config
  config/modify				| /v1/config
  objects/query/;&lt;type;&gt;		| /v1/objects
  objects/create/;&lt;type;&gt;		| /v1/objects
  objects/modify`/;&lt;type;&gt;	| /v1/objects
  objects/delete/;&lt;type;&gt;		| /v1/objects
  status/query				| /v1/status

The required actions or types can be replaced by using a wildcard match ("*").

### <a id="icinga2-api-parameters"></a> Parameters

Depending on the request method there are two ways of
passing parameters to the request:

* JSON body (`POST`, `PUT`)
* Query string (`GET`, `DELETE`)

Reserved characters by the HTTP protocol must be passed url-encoded as query string, e.g. a
space becomes `%20`.

Example for query string:

    /v1/objects/hosts?filter=match(%22nbmif*%22,host.name)&attrs=host.name&attrs=host.state

Example for JSON body:

    { "attrs": { "address": "8.8.4.4", "vars.os" : "Windows" } }


#### <a id="icinga2-api-filters"></a> Filters

Use the same syntax as for apply rule expressions
for filtering specific objects.

Example for all services in NOT-OK state:

    https://localhost:5665/v1/objects/services?filter=service.state!=0

Example for matching all hosts by name (**Note**: `"` are url-encoded as `%22`):

    https://localhost:5665/v1/objects/hosts?filter=match(%22nbmif*%22,host.name)


### <a id="icinga2-api-output-format"></a>Output Format

The request and reponse body contain a JSON encoded string.

### <a id="icinga2-api-version"></a>Version

Each url contains the version string as prefix (currently "/v1").

### <a id="icinga2-api-url-endpoints"></a>Url Endpoints

The Icinga 2 API provides multiple url endpoints:

  Url Endpoints	| Description
  --------------|----------------------------------------------------
  /v1/actions	| Endpoint for running specific [API actions](9-icinga2-api.md#icinga2-api-actions).
  /v1/config    | Endpoint for [managing configuration modules](9-icinga2-api.md#icinga2-api-config-management).
  /v1/events	| Endpoint for subscribing to [API events](9-icinga2-api.md#icinga2-api-actions).
  /v1/objects	| Endpoint for querying, creating, modifying and deleting [config objects](9-icinga2-api.md#icinga2-api-config-objects).
  /v1/status	| Endpoint for receiving icinga2 [status and statistics](9-icinga2-api.md#icinga2-api-status).
  /v1/types 	| Endpoint for listing Icinga 2 configuration object types and their attributes.

Please check the respective sections for detailed urls and parameters.

## <a id="icinga2-api-actions"></a> Actions
There are several actions available for Icinga 2 provided by the `actions` 
URL endpoint. In case you have been using the [external commands](5-advanced-topics.md#external-commands) 
in the past, the API actions provide a similar interface with filter 
capabilities for some of the more common targets which do not directly change 
the configuration. Some actions require specific target types (e.g. `type=Host`) 
and a [filter expression](9-icinga2-api.md#icinga2-api-filters).  
For each object matching the filter the action in question is performed once.

In the following each the actions are listed with their parameters, targets and 
examples. The calls are first shown with all their possible query parameters and 
their type. Optional parameters are encapsulated by `()` and `[]` mark array 
parameters. If an optional parameter has no default value explicitly stated it 
is either 0, NULL, Empty depending on the type. Timestamps are always `time_t`, 
the seconds since the UNIX epoch.

All actions return a 200 `OK` or an appropriate error code for each 
action performed. So there will be a return for each object matching the filter.

### process-check-result

    /v1/actions/process-check-result?exit_status=int&(plugin_output=string)&(performance_data[]=string)&\
	(check_command[]=string)&(check_source=string)&(execution_end=time_t)&(execution_start=time_t)&\
	(schedule_end=time_t)&(schedule_start=time_t)
    
Target: `Service` or `Host`

This is used to submit a passive check result for a service or host. Passive 
checks need to be enabled for the check result to be processed. 
The `exit_status` field should be one of the following for services: 
0=OK, 1=WARNING, 2=CRITICAL, 3=UNKNOWN or for hosts: 0=OK, 1=CRITICAL. 
The `plugin_output` field contains text output from the service check, the 
performance data is submitted via `performance_data` as one array entry per ';' 
separated block. 

Example:

    $ curls -u root:icinga -k -s 'https://localhost:5665/v1/actions/process-check-result?filter=service.name=="ping6"'
    
### reschedule-check

    /v1/actions/reschedule-check?next_check=time_t&force_check=boolean

Target: `Service` or `Host`

Schedules an active check of a collection of hosts or services at `next_check`. 
If the `forced_check" flag is set the checks are performed regardless of what 
time it is (e.g. timeperiod restrictions are ignored) and whether or not active 
checks are enabled on a host/service-specific or program-wide basis.

### send-custom-notification

    /v1/actions/send-custom-notification?author=string&comment=string&(force=bool)

Target: `Service` or `Host`

Allows you to send a custom host/service notification. Very useful in dire 
situations, emergencies or to communicate with all admins that are responsible 
for a host or service. The notification requires an `author` and a `comment`, 
though those may be empty. If `force` (default: false) is set to true the 
notification will be send regardless of downtimes or whether notifications are 
enabled or not.

### delay-notification

    /v1/actions/delay-notification?timestamp=time_t

Target: `Service` or `Host`

Delays the next notifications for a collection of services or hosts until 
`timestamp`. Note that this will only have an effect if the service stays in 
the same problem state that it is currently in. If the service changes to 
another state, a new notification may go out before the time you specify in the 
`timestamp` argument.

### acknowledge-problem

    /v1/actions/acknowledge-problem?author=string&comment=string&(expiry=time_t)&(sticky=bool)&(sticky=bool)

Target: `Service` or `Host`

Allows you to acknowledge the current problem for hosts or services. By 
acknowledging the current problem, future notifications (for the same state) 
are disabled. Acknowledgements require an `author` and a `comment` for 
documentation purposes, though both may be empty. If you set an `expiry` time 
the acknowledgement will vanish after that timestamp. If the `sticky` option is 
set (the default), the acknowledgement will remain until the host recovers. 
Otherwise the acknowledgement will automatically be removed when the host 
changes state. If the `notify` option is set, a notification will be sent out 
to contacts indicating that the current host problem has been acknowledged, if 
set to false (the default) there will be no notification.

### remove-acknowledgement

    /v1/actions/remove-acknowledgement

Target: `Service` or `Host`

Removes acknowledgements for services or hosts. Once the acknowledgement has 
been removed, notifications can once again be sent out.

### add-comment

    /v1/actions/add-comment?author=string&comment=string
    
Target: `service` or `host`

Adds a `comment` by `author` to services or hosts.

### remove-comment

    /v1/actions/remove-comment
    
Target: `Service` or `Host`

Removes ALL comments for services or hosts.

### remove-comment-by-id

    /v1/actions/remove-comment-by-id?comment_id=int
    
Target: `None`

Removes the comment with the legacy ID `comment_id`

### schedule-downtime

    /v1/actions/schedule-downtime?start_time=time_t&end_time=time_t&duration=int&author=string&comment=string&\
    (fixed=bool)&(trigger_id=int)

Target: `Host` or `Service`

Schedules downtime for services or hosts. If the `fixed` argument is set to 
true (default: false) the downtime will start and end at the times specified by 
the `start_time` and `end_time` arguments. Otherwise, downtime will begin 
between `start_time` and `start_end` and last for `duration` seconds. The 
downtime can be triggered by another downtime entry if the `trigger_id` is set 
to the ID of another scheduled downtime entry. Set the `trigger_id` argument to 
zero (the default) if the downtime for the specified host should not be 
triggered by another downtime entry. All downtimes also need a `comment` and 
with it an `author`, even though both can be empty.

### remove-downtime

    /v1/actions/remove-downtime

Target: `Host` or `Service`

Removes ALL downtimes for services or hosts.

### remove-downtime-by-id

    /v1/actions/remove-downtime-by-id?downtime_id=int

Target: `None`

Removes the comment with the legacy ID `downtime_id`

### shutdown-process

    /v1/actions/shutdown-process

Target: `None`

Shuts down Icinga2. May or may not return.

### restart-process

    /v1/actions/restart-process

Target: `None`

Restarts Icinga2. May or may not return.

## <a id="icinga2-api-event-streams"></a> Event Streams

**TODO** https://dev.icinga.org/issues/9078

## <a id="icinga2-api-status"></a> Status and Statistics

Contains a list of sub url endpoints which provide the status and statistics
of available and enabled features. Any filters are ignored.

Example for the main url endpoint `/v1/status`:

    $ curl -k -s -u root:icinga 'https://localhost:5665/v1/status' | python -m json.tool
    {
        "results": [
            {
                "name": "ApiListener",
				"perfdata": [ ... ],
				"status": [ ... ]
            },
            ...
            {
                "name": "IcingaAplication",
				"perfdata": [ ... ],
				"status": [ ... ]
            },
            ...
        ]
    }

`/v1/status` is always available as virtual status url endpoint.
It provides all feature status information into a collected overview.

Example for the icinga application url endpoint `/v1/status/IcingaApplication`:

    $ curl -k -s -u root:icinga 'https://localhost:5665/v1/status/IcingaApplication' | python -m json.tool
    {
        "results": [
            {
                "perfdata": [],
                "status": {
                    "icingaapplication": {
                        "app": {
                            "enable_event_handlers": true,
                            "enable_flapping": true,
                            "enable_host_checks": true,
                            "enable_notifications": true,
                            "enable_perfdata": true,
                            "enable_service_checks": true,
                            "node_name": "icinga.org",
                            "pid": 59819.0,
                            "program_start": 1443019345.093372,
                            "version": "v2.3.0-573-g380a131"
                        }
                    }
                }
            }
        ]
    }


## <a id="icinga2-api-config-objects"></a> Config Objects

Provides functionality for all configuration object url endpoints
provided by [config object types](6-object-types.md#object-types):

  Url Endpoints				| Description
  --------------------------------------|----------------------------------------------------
  /v1/objects/hosts			| Endpoint for retreiving and updating [Host](6-object-types.md#objecttype-host) objects.
  /v1/objects/services			| Endpoint for retreiving and updating [Service](6-object-types.md#objecttype-service) objects.
  /v1/objects/notifications		| Endpoint for retreiving and updating [Notification](6-object-types.md#objecttype-notification) objects.
  /v1/objects/dependencies		| Endpoint for retreiving and updating [Dependency](6-object-types.md#objecttype-dependency) objects.
  /v1/objects/users			| Endpoint for retreiving and updating [User](6-object-types.md#objecttype-user) objects.
  /v1/objects/checkcommands		| Endpoint for retreiving and updating [CheckCommand](6-object-types.md#objecttype-checkcommand) objects.
  /v1/objects/eventcommands		| Endpoint for retreiving and updating [EventCommand](6-object-types.md#objecttype-eventcommand) objects.
  /v1/objects/notificationcommands	| Endpoint for retreiving and updating [NotificationCommand](6-object-types.md#objecttype-notificationcommand) objects.
  /v1/objects/hostgroups		| Endpoint for retreiving and updating [HostGroup](6-object-types.md#objecttype-hostgroup) objects.
  /v1/objects/servicegroups		| Endpoint for retreiving and updating [ServiceGroup](6-object-types.md#objecttype-servicegroup) objects.
  /v1/objects/usergroups		| Endpoint for retreiving and updating [UserGroup](6-object-types.md#objecttype-usergroup) objects.
  /v1/objects/zones			| Endpoint for retreiving and updating [Zone](6-object-types.md#objecttype-zone) objects.
  /v1/objects/endpoints			| Endpoint for retreiving and updating [Endpoint](6-object-types.md#objecttype-endpoint) objects.
  /v1/objects/timeperiods		| Endpoint for retreiving and updating [TimePeriod](6-object-types.md#objecttype-timeperiod) objects.

All object attributes are prefixed with their respective object type.

Example:

    host.address

Output listing and url parameters use the same syntax.

### <a id="icinga2-api-config-objects-joins"></a> API Objects and Joins

Icinga 2 knows about object relations, e.g. when querying a service object
the query handler will automatically add the referenced host object and its
attributes to the result set. If the object reference is null (e.g. no event_command
defined), the joined results not added to the result set.

**Note**: Select your required attributes beforehand by passing them to your
request. The default result set might get huge.

Each joined object will use its own attribute name as prefix for the attribute.
There is an exception for multiple objects used in dependencies and zones.

Objects with optional relations (e.g. a host notification does not have services)
will not be joined.

  Object Type	| Object Relations (prefix name)
  --------------|---------------------------------
  Service	| host, notification, check_command, event_command
  Host		| notification, check_command, event_command
  Notification  | host, service, command, period
  Dependency 	| child_host, child_service, parent_host, parent_service, period
  User		| period
  Zones		| parent


### <a id="icinga2-api-config-objects-cluster-sync"></a> API Objects and Cluster Config Sync

Newly created or updated objects can be synced throughout your
Icinga 2 cluster. Set the `zone` attribute to the zone this object
belongs to and let the API and cluster handle the rest.

If you add a new cluster instance, or boot an instance beeing offline
for a while, Icinga 2 takes care of the initial object sync for all
objects created by the API.

More information about distributed monitoring, cluster and its
configuration can be found [here](13-distributed-monitoring-ha.md#distributed-monitoring-high-availability).


### <a id="icinga2-api-config-objects-list"></a> List All Objects

Send a `GET` request to `/v1/objects/hosts` to list all host objects and
their attributes.

    $ curl -u root:icinga -k -s 'https://localhost:5665/v1/objects/hosts'

This works in a similar fashion for other [config objects](9-icinga2-api.md#icinga2-api-config-objects).


#### <a id="icinga2-api-objects-create"></a> Create New Config Object

New objects must be created by sending a PUT request. The following
parameters need to be passed inside the JSON body:

  Parameters	| Description
  --------------|------------------------------------
  name		| **Required.** Name of the newly created config object.
  templates	| **Optional.** Import existing configuration templates for this object type.
  attrs		| **Required.** Set specific object attributes for this [object type](6-object-types.md#object-types).


If attributes are of the Dictionary type, you can also use the indexer format:

    "attrs": { "vars.os": "Linux" }

Example fo creating the new host object `google.com`:

    $ curl -u root:icinga -k -s 'https://localhost:5665/v1/objects/hosts/google.com' \
    -X PUT \
    -d '{ "templates": [ "generic-host" ], "attrs": { "address": "8.8.8.8", "check_command": "hostalive", "vars.os" : "Linux" } }' \
    | python -m json.tool
    {
        "results": [
            {
                "code": 200.0,
                "status": "Object was created."
            }
        ]
    }

**Note**: Host objects require the `check_command` attribute.

If the configuration validation fails, the new object will not be created and the response body
contains a detailed error message. The following example omits the `check_command` attribute required
by the host object.

    $ curl -u root:icinga -k -s 'https://localhost:5665/v1/objects/hosts/google.com' \
    -X PUT \
    -d '{ "attrs": { "address": "8.8.8.8", "vars.os" : "Linux" } }' \
    | python -m json.tool
    {
        "results": [
            {
                "code": 500.0,
                "errors": [
                    "Error: Validation failed for object 'google.com' of type 'Host'; Attribute 'check_command': Attribute must not be empty."
                ],
                "status": "Object could not be created."
            }
        ]
    }

#### <a id="icinga2-api-object-query"></a> Query Object

Send a `GET` request including the object name inside the url.

Example for the host `google.com`:

    $ curl -u root:icinga -k -s 'https://localhost:5665/v1/objects/hosts/google.com'

You can select specific attributes by adding them as url parameters using `?attrs=...`. Multiple
attributes must be added one by one, e.g. `?attrs=host.address&attrs=host.name`.

    $ curl -u root:icinga -k -s 'https://localhost:5665/v1/objects/hosts/google.com?attrs=host.name&attrs=host.address' | python -m json.tool
    {
        "results": [
            {
                "attrs": {
                    "host.address": "8.8.8.8",
                    "host.name": "google.com"
                }
            }
        ]
    }

#### <a id="icinga2-api-objects-modify"></a> Modify Object

Existing objects must be modifed by sending a `POST` request. The following
parameters need to be passed inside the JSON body:

  Parameters	| Description
  --------------|------------------------------------
  name		| **Optional.** If not specified inside the url, this is **required**.
  templates	| **Optional.** Import existing object configuration templates.
  attrs		| **Required.** Set specific object attributes for this [object type](6-object-types.md#object-types).


If attributes are of the Dictionary type, you can also use the indexer format:

    "attrs": { "vars.os": "Linux" }


Example for existing object `google.com`:

    $ curl -u root:icinga -k -s 'https://localhost:5665/v1/objects/hosts/google.com' \
    -X POST \
    -d '{ "attrs": { "address": "8.8.4.4", "vars.os" : "Windows" } }' \
    | python -m json.tool
    {
        "results": [
            {
                "code": 200.0,
                "name": "google.com",
                "status": "Attributes updated.",
                "type": "Host"
            }
        ]
    }

#### <a id="icinga2-api-hosts-delete"></a> Delete Host

You can delete objects created using the API by sending a `DELETE`
request. Specify the object name inside the url.

  Parameters	| Description
  --------------|------------------------------------
  cascade	| **Optional.** Delete objects depending on the deleted objects (e.g. services on a host).

**Note**: Objects created by apply rules (services, notifications, etc) will implicitely require
to pass the `cascade` parameter on host object deletion.

Example for deleting the host object `google.com`:

    $ curl -u root:icinga -k -s 'https://localhost:5665/v1/objects/hosts/google.com?cascade=1' -X DELETE | python -m json.tool
    {
        "results": [
            {
                "code": 200.0,
                "name": "google.com",
                "status": "Object was deleted.",
                "type": "Host"
            }
        ]
    }


## <a id="icinga2-api-config-management"></a> Configuration Management

The main idea behind configuration management is to allow external applications
creating configuration packages and stages based on configuration files and
directory trees. This replaces any additional SSH connection and whatnot to
dump configuration files to Icinga 2 directly.
In case you are pushing a new configuration stage to a package, Icinga 2 will
validate the configuration asynchronously and populate a status log which
can be fetched in a separated request.

### <a id="icinga2-api-config-management-create-package"></a> Create Config Package

Send a `POST` request to a new config package called `puppet` in this example. This
will create a new empty configuration package.

    $ curl -k -s -u root:icinga -X POST https://localhost:5665/v1/config/packages/puppet | python -m json.tool
    {
        "results": [
            {
                "code": 200.0,
                "package": "puppet",
                "status": "Created package."
            }
        ]
    }

### <a id="icinga2-api-config-management-create-config-stage"></a> Create Configuration to Package Stage

Send a `POST` request to the url endpoint `/v1/config/stages` including an existing
configuration package, e.g. `puppet`.
The request body must contain the `files` attribute with the value being
a dictionary of file targets and their content.

The example below will create a new file called `test.conf` underneath the `conf.d`
directory populated by the sent configuration.
The Icinga 2 API returns the `package` name this stage was created for, and also
generates a unique name for the `package` attribute you'll need for later requests.

Note: This example contains an error (`chec_command`), do not blindly copy paste it.

    $ curl -k -s -u root:icinga -X POST -d '{ "files": { "conf.d/test.conf": "object Host \"cfg-mgmt\" { chec_command = \"dummy\" }" } }' https://localhost:5665/v1/config/stages/puppet | python -m json.tool
    {
        "results": [
            {
                "code": 200.0,
                "package": "puppet",
                "stage": "nbmif-1441625839-0",
                "status": "Created stage."
            }
        ]
    }

If the configuration fails, the old active stage will remain active.
If everything is successful, the new config stage is activated and live.
Older stages will still be available in order to have some sort of revision
system in place.

Icinga 2 automatically creates the following files in the main configuration package
stage:

  File		| Description
  --------------|---------------------------
  status	| Contains the [configuration validation](8-cli-commands.md#config-validation) exit code (everything else than 0 indicates an error).
  startup.log	| Contains the [configuration validation](8-cli-commands.md#config-validation) output.

You can [fetch these files](9-icinga2-api.md#icinga2-api-config-management-fetch-config-package-stage-files) via API call
after creating a new stage.

### <a id="icinga2-api-config-management-list-config-packages"></a> List Configuration Packages and their Stages

List all config packages, their active stage and other stages.
That way you may iterate of all of them programmatically for
older revisions and their requests.

The following example contains one configuration package `puppet`.
The latter already has a stage created, but it is not active.

    $ curl -k -s -u root:icinga https://localhost:5665/v1/config/packages | python -m json.tool
    {
        "results": [
            {
                "active-stage": "",
                "name": "puppet",
                "stages": [
                    "nbmif-1441625839-0"
                ]
            }
        ]
    }

### <a id="icinga2-api-config-management-list-config-package-stage-files"></a> List Configuration Packages and their Stages

Sent a `GET` request to the url endpoint `/v1/config/stages` including the package
(`puppet`) and stage (`nbmif-1441625839-0`) name.

    $ curl -k -s -u root:icinga https://localhost:5665/v1/config/stages/puppet/nbmif-1441625839-0 | python -m json.tool
    {
        "results": [
    ...
            {
                "name": "startup.log",
                "type": "file"
            },
            {
                "name": "status",
                "type": "file"
            },
            {
                "name": "conf.d",
                "type": "directory"
            },
            {
                "name": "zones.d",
                "type": "directory"
            },
            {
                "name": "conf.d/test.conf",
                "type": "file"
            }
        ]
    }


### <a id="icinga2-api-config-management-fetch-config-package-stage-files"></a> Fetch Configuration Package Stage Files

Send a `GET` request to the url endpoint `/v1/config/files` including
the package name, the stage name and the relative path to the file.
Note: You cannot use dots in paths.

You can fetch a [list of existing files](9-icinga2-api.md#icinga2-api-config-management-list-config-package-stage-files)
in a configuration stage and then specifically request their content.

The following example fetches the faulty configuration inside `conf.d/test.conf`
for further analysis.

    $ curl -k -s -u root:icinga https://localhost:5665/v1/config/files/puppet/nbmif-1441625839-0/conf.d/test.conf
    object Host "cfg-mgmt" { chec_command = "dummy" }

Note: The returned files are plain-text instead of JSON-encoded.

### <a id="icinga2-api-config-management-config-package-stage-errors"></a> Configuration Package Stage Errors

Now that we don’t have an active stage for `puppet` yet seen [here](9-icinga2-api.md#icinga2-api-config-management-list-config-packages),
there must have been an error.

Fetch the `startup.log` file and check the config validation errors:

    $ curl -k -s -u root:icinga https://localhost:5665/v1/config/files/puppet/imagine-1441133065-1/startup.log
    ...

    critical/config: Error: Attribute 'chec_command' does not exist.
    Location:
    /var/lib/icinga2/api/packages/puppet/imagine-1441133065-1/conf.d/test.conf(1): object Host "cfg-mgmt" { chec_command = "dummy" }
                                                                                                           ^^^^^^^^^^^^^^^^^^^^^^

    critical/config: 1 error

The output is similar to the manual [configuration validation](8-cli-commands.md#config-validation).

