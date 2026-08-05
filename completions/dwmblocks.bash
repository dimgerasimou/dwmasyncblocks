# bash completion for dwmblocks
# Install as: $BASH_COMPLETION_USER_DIR/completions/dwmblocks
#         or: /usr/share/bash-completion/completions/dwmblocks
# Requires the bash-completion package (for _init_completion).

_dwmblocks()
{
	local cur prev words cword split
	_init_completion -s || return

	case "$prev" in
	--update)
		local IFS=$'\n'
		COMPREPLY=( $(compgen -W \
			"$(dwmblocks --list 2>/dev/null)" -- "$cur") )
		return
		;;
	esac

	# Handled a --opt=value split that matched no case above.
	$split && return

	if [[ $cur == -* ]]; then
		COMPREPLY=( $(compgen -W '--all --restart --list --update --version --help' \
			-- "$cur") )
	fi
}
complete -F _dwmblocks dwmblocks
