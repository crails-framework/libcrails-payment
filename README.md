# libcrails-payment - A C++ library

The `libcrails-payment` C++ library provides <SUMMARY-OF-FUNCTIONALITY>.


## Usage

To start using `libcrails-payment` in your project, add the following `depends`
value to your `manifest`, adjusting the version constraint as appropriate:

```
depends: libcrails-payment ^<VERSION>
```

Then import the library in your `buildfile`:

```
import libs = libcrails-payment%lib{<TARGET>}
```


## Importable targets

This package provides the following importable targets:

```
lib{<TARGET>}
```

<DESCRIPTION-OF-IMPORTABLE-TARGETS>


## Configuration variables

This package provides the following configuration variables:

```
[bool] config.libcrails_payment.<VARIABLE> ?= false
```

<DESCRIPTION-OF-CONFIG-VARIABLES>
