fastgit() {
    case $1 in
    clone)
        shift
        if [ -z "$2" ]; then
            gix clone "$1" "./$(getPathFromUrl $1)" --bare
        else
            command gix clone "$@" --bare
        fi
        ;;
    *)
        command git "$@"
        ;;
    esac
}
