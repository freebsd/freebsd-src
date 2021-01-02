module.exports = {
    "env": {
        "es6": true,
        "node": true,
        "browser": false
    },
    "extends": "eslint:recommended",
    "parserOptions": {
        "sourceType": "module"
    },
    "rules": {
        "indent": [
            "error",
            "tab"
        ],
        "linebreak-style": [
            "error",
            "unix"
        ],
        "quotes": [
            "warn",
            "single"
        ],
        "semi": [
            "error",
            "always"
        ],
        "curly": [
            "error",
            "all"
        ],
        "brace-style": [
            "error",
            "1tbs"
        ],
        "no-empty" : "warn",
        "no-unused-vars" : "warn",
        "no-console": "warn",
        "consistent-return": "error",
        "class-methods-use-this": "warn",
        "eqeqeq": [
            "error",
            "always", {
                "null": "ignore"
            }
        ],
        "no-alert": "warn",
        "no-caller": "error",
        "no-eval": "error",
        "no-extend-native": "warn",
        "no-implicit-coercion": "error",
        "no-implied-eval": "error",
        "no-invalid-this": "error",
        "no-loop-func": "error",
        "no-new-func": "warn",
        "no-new-wrappers": "error",
        "no-proto": "error",
        "no-return-assign": "warn",
        "no-return-await": "warn",
        "no-script-url": "error",
        "no-self-compare": "error",
        "no-sequences": "error",
        "no-throw-literal": "error",
        "no-unmodified-loop-condition": "warn",
        "no-unused-expressions": "warn",
        "no-useless-return": "warn",
        "no-warning-comments": "warn",
        "prefer-promise-reject-errors": "warn",
        "no-label-var": "error",
        "no-shadow": [
            "warn", {
                "builtinGlobals": true,
                "hoist": "all"
            }
        ],
        "no-shadow-restricted-names": "error",
        "no-undefined": "error",
        "no-use-before-define": "error",
        "no-new-require": "error",
        "no-path-concat": "error",
        "camelcase": "error",
        "comma-dangle": [
            "error",
            "never"
        ],
        "eol-last": [
            "error",
            "always"
        ],
        "func-call-spacing": "warn",
        "lines-around-directive": [
            "warn",
            "always"
        ],
        "max-params": [
            "warn", {
                "max": 7
            }
        ],
        "max-statements-per-line": [
            "warn", {
                "max": 1
            }
        ],
        "new-cap": [
            "error"
        ],
        "no-array-constructor": "warn",
        "no-mixed-operators": [
            "error", {
                "groups": [
                    ["&", "|", "^", "~", "<<", ">>", ">>>"],
                    ["==", "!=", "===", "!==", ">", ">=", "<", "<="],
                    ["&&", "||"],
                    ["in", "instanceof"]
                ],
                "allowSamePrecedence": false
            }
        ],
        "no-trailing-spaces": "warn",
        "no-unneeded-ternary": "warn",
        "no-whitespace-before-property": "error",
        "operator-linebreak": "warn",
        "semi-spacing": "warn",
        "no-confusing-arrow": [
            "error", {
                "allowParens": true
            }
        ],
        "no-duplicate-imports": "warn",
        "prefer-rest-params": "warn",
        "prefer-spread": "warn",
        "no-unsafe-negation": "warn"
    }
};
